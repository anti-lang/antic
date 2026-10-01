/* The licence notice of every Anti binary, shared by antic, the runtime
   and anti.

   DESIGN: one definition read from three sides. antic writes the notice
   between the two markers, with the line `build <id>` right after the
   begin marker. The runtime gives the text between the markers without
   that line, and a trace reads the id. `anti build` and `anti symbols`
   find the id of a binary at the begin marker, and nowhere else, since
   the data of a program may hold a line of the same form. src/antic/
   notice.h includes this header rather than repeating the markers. */
#ifndef ANTI_RT_LICENSE_H
#define ANTI_RT_LICENSE_H

#define ANTI_NOTICE_BEGIN "ANTI_LICENSES_BEGIN\n"
#define ANTI_NOTICE_END "ANTI_LICENSES_END\n"
/* The head of the line after the begin marker. The id follows it, then
   a newline. */
#define ANTI_NOTICE_BUILD "build "
/* The digits of a build id, the SHA-256 digest in lowercase hex. */
#define ANTI_BUILD_ID_LENGTH 64

#endif
