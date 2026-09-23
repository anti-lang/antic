/* The C half of the tests sqlite_run and sqlite_link_<target>. It queries
   an in-memory database with the SQLite of the runtime tree. The functions
   give plain integers, so the Anti half needs no binding of SQLite. */
#include "../binary_stdio.h"
#include <sqlite3.h>
#include <stdint.h>
#include <string.h>

int32_t sqlite_probe_version(void);
int32_t sqlite_probe_threadsafe(void);
int32_t sqlite_probe_query(const char *sql);

/* 1 when the library reports the version of its header. */
int32_t sqlite_probe_version(void)
{
    return sqlite3_libversion_number() == SQLITE_VERSION_NUMBER &&
           strcmp(sqlite3_libversion(), SQLITE_VERSION) == 0;
}

/* The threading mode the library was compiled with, 1 for serialized. */
int32_t sqlite_probe_threadsafe(void)
{
    return sqlite3_threadsafe();
}

/* Opens an empty in-memory database and fills a table through a prepared
   statement with bound values. The result is the integer of the first
   column of the first row of <sql>. A failing step gives -1000 minus the result
   code of SQLite, and a query without a row gives -1. */
int32_t sqlite_probe_query(const char *sql)
{
    static const char *const names[3] = {"ein", "zwei", "drei"};
    static const int values[3] = {3, 12, 27};
    sqlite3 *db = NULL;
    sqlite3_stmt *insert = NULL;
    sqlite3_stmt *query = NULL;
    int32_t result = -1;
    int code;
    int i;

    code = sqlite3_open_v2(":memory:", &db,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (code == SQLITE_OK) {
        code = sqlite3_exec(db, "CREATE TABLE t (name TEXT, n INTEGER)", NULL,
                            NULL, NULL);
    }
    if (code == SQLITE_OK) {
        code = sqlite3_prepare_v2(db, "INSERT INTO t VALUES (?, ?)", -1,
                                  &insert, NULL);
    }
    for (i = 0; code == SQLITE_OK && i < 3; i++) {
        code = sqlite3_bind_text(insert, 1, names[i], -1, SQLITE_STATIC);
        if (code == SQLITE_OK) {
            code = sqlite3_bind_int(insert, 2, values[i]);
        }
        if (code == SQLITE_OK) {
            code = sqlite3_step(insert);
            code = code == SQLITE_DONE ? sqlite3_reset(insert) : code;
        }
    }
    if (code == SQLITE_OK) {
        code = sqlite3_prepare_v2(db, sql, -1, &query, NULL);
    }
    if (code == SQLITE_OK) {
        code = sqlite3_step(query);
        if (code == SQLITE_ROW) {
            result = (int32_t)sqlite3_column_int(query, 0);
            code = SQLITE_OK;
        } else if (code == SQLITE_DONE) {
            code = SQLITE_OK;
        }
    }
    if (code != SQLITE_OK) {
        result = -1000 - (int32_t)code;
    }
    sqlite3_finalize(query);
    sqlite3_finalize(insert);
    sqlite3_close(db);
    return result;
}
