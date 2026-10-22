# Drive the resolution of `anti build` in two parts: over a `file://`
# repository whose picks change while the walk runs, and over a manifest
# of more than one repository that a dependency is searched in. Run with
# cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   HOST     the target name of this host
#   WORK     a directory this run writes into
#
# A third part takes a graph in which two packages need a third, beside
# a dependency of a path, and a lock file whose needs, modules, digests
# or packages the graph no longer gives.
#
# A fourth part takes back a pick whose two dependencies are both in the
# graph, one of which needs the other.
#
# The first part: the project needs a 1.x and b 1.x. The walk first picks a 1.1.0, which
# needs c =2.0.0 and d 1.0.0. Then b 1.0.0 needs a =1.0.0, so a goes back
# to 1.0.0, which needs c =1.0.0 and no d. The requirements of the first
# pick of a leave with it: c settles on 1.0.0, d leaves the graph, and
# the lock file names a 1.0.0, b 1.0.0 and c 1.0.0 alone. The resolver
# kept every requirement it had read, so c =2.0.0 and c =1.0.0 refused
# the project.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

function(file_url path out)
    if(path MATCHES "^/")
        set(${out} "file://${path}" PARENT_SCOPE)
    else()
        set(${out} "file:///${path}" PARENT_SCOPE)
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(run_anti_status where out_status out_text)
    anti_run(status out err "${where}" ${ARGN} --runtime "${RUNTIME}")
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(run_anti where)
    anti_run(status out err "${where}" ${ARGN} --runtime "${RUNTIME}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti ${ARGN} in ${where} failed with "
                            "${status}\n${out}${err}")
    endif()
endfunction()

# One library project per package, whose library file every version of
# the package in the repository carries. The index decides the
# dependencies of each version, which is what the resolver reads.
foreach(package a b c d)
    set(project "${WORK}/${package}")
    file(WRITE "${project}/anti.toml"
         "[package]\n"
         "name = \"com.example.${package}\"\n"
         "version = \"1.0.0\"\n")
    file(WRITE "${project}/src/com/example/${package}.anti"
         "//! A package of the resolution test.\n"
         "\n"
         "/// One.\n"
         "pub fn one() -> int\n"
         "{\n"
         "\treturn 1;\n"
         "}\n")
    run_anti("${project}" build)
    set(library_${package}
        "${project}/dist/${HOST}/dev/com/example/${package}.antl")
    file(SHA256 "${library_${package}}" digest_${package})
endforeach()

# Publish package at each version, and write its index from entries.
set(repo "${WORK}/repo")
function(publish package entries)
    foreach(version IN LISTS ARGN)
        set(at "${repo}/com.example.${package}/${version}")
        file(MAKE_DIRECTORY "${at}")
        file(COPY_FILE "${library_${package}}"
             "${at}/com.example.${package}.antl")
    endforeach()
    file(WRITE "${repo}/com.example.${package}/index.toml"
         "name = \"com.example.${package}\"\n"
         "revision = 1\n"
         "${entries}")
endfunction()

# One version entry of package with the TOML of its dependencies.
function(entry out package version dependencies)
    set(text "${${out}}")
    string(APPEND text "\n[[version]]\nversion = \"${version}\"\n"
           "modules = [{ path = \"com.example.${package}\", "
           "sha256 = \"${digest_${package}}\" }]\n"
           "dependencies = [${dependencies}]\n")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

set(a "")
entry(a a 1.0.0 "{ name = \"com.example.c\", version = \"=1.0.0\" }")
entry(a a 1.1.0 "{ name = \"com.example.c\", version = \"=2.0.0\" }, { name = \"com.example.d\", version = \"1.0.0\" }")
publish(a "${a}" 1.0.0 1.1.0)
set(b "")
entry(b b 1.0.0 "{ name = \"com.example.a\", version = \"=1.0.0\" }")
publish(b "${b}" 1.0.0)
set(c "")
entry(c c 1.0.0 "")
entry(c c 2.0.0 "")
publish(c "${c}" 1.0.0 2.0.0)
set(d "")
entry(d d 1.0.0 "")
publish(d "${d}" 1.0.0)

set(project "${WORK}/app")
file_url("${repo}" repo_url)
file(WRITE "${project}/anti.toml"
     "[package]\n"
     "name = \"com.example.app\"\n"
     "version = \"0.1.0\"\n"
     "\n"
     "[repositories]\n"
     "local = \"${repo_url}\"\n"
     "\n"
     "[dependencies]\n"
     "\"com.example.a\" = { version = \"1.0.0\", repo = \"local\" }\n"
     "\"com.example.b\" = { version = \"1.0.0\", repo = \"local\" }\n")
file(WRITE "${project}/src/com/example/app.anti"
     "//! A program over a, b and the packages they need.\n"
     "\n"
     "import com.example.a;\n"
     "import com.example.b;\n"
     "\n"
     "fn main() -> int\n"
     "{\n"
     "\treturn a.one() + b.one();\n"
     "}\n")
run_anti("${project}" build)
program_expect("the program" COMMAND "${project}/dist/${HOST}/dev/app"
               STATUS 2 OUT "")
file(READ "${project}/anti.lock" lock)
foreach(pick "a\"\nversion = \"1.0.0" "b\"\nversion = \"1.0.0"
        "c\"\nversion = \"1.0.0")
    if(NOT lock MATCHES "name = \"com.example.${pick}\"")
        message(FATAL_ERROR "the lock file names no com.example.${pick}:\n"
                            "${lock}")
    endif()
endforeach()
if(lock MATCHES "com.example.d")
    message(FATAL_ERROR "the lock file keeps com.example.d, which only the "
                        "dropped pick of a needs:\n${lock}")
endif()

# The second part: a dependency that names no repository is searched for
# in every repository of the manifest, and two that hold it are an error.
# A repository that holds no index of the package is no answer for it,
# and the build goes on. One whose index cannot be read is no answer at
# all, and the build stops there, since that repository may hold the
# package too. The resolver took such a repository as one without the
# package and built from the other.
set(empty "${WORK}/empty")
set(broken "${WORK}/broken")
file(MAKE_DIRECTORY "${empty}" "${broken}/com.example.c")
file(WRITE "${broken}/com.example.c/index.toml" "[[version\n")
file_url("${empty}" empty_url)
file_url("${broken}" broken_url)
set(project "${WORK}/search")
function(write_search second)
    file(WRITE "${project}/anti.toml"
         "[package]\n"
         "name = \"com.example.search\"\n"
         "version = \"0.1.0\"\n"
         "\n"
         "[repositories]\n"
         "local = \"${repo_url}\"\n"
         "other = \"${second}\"\n"
         "\n"
         "[dependencies]\n"
         "\"com.example.c\" = { version = \"1.0.0\" }\n")
endfunction()
file(WRITE "${project}/src/com/example/search.anti"
     "//! A program over c, from whichever repository holds it.\n"
     "\n"
     "import com.example.c;\n"
     "\n"
     "fn main() -> int\n"
     "{\n"
     "\treturn c.one();\n"
     "}\n")
write_search("${empty_url}")
run_anti("${project}" build)
file(READ "${project}/anti.lock" lock)
if(NOT lock MATCHES "name = \"com.example.c\"\nversion = \"1.0.0\"")
    message(FATAL_ERROR "the lock file names no com.example.c 1.0.0:\n"
                        "${lock}")
endif()
file(REMOVE "${project}/anti.lock")
write_search("${broken_url}")
run_anti_status("${project}" status text build)
if(status EQUAL 0)
    message(FATAL_ERROR "the build took a repository whose index of "
                        "com.example.c cannot be read as one without it:\n"
                        "${text}")
endif()
if(NOT text MATCHES "gave no index of com.example.c that anti reads")
    message(FATAL_ERROR "the build said\n${text}")
endif()

# The third part: e and f each need g, f needs e as well, and the
# project needs e, f and the project of c by its path. The lock file
# names g once, with the needs of e and f. The index of f names g three
# times, once without a version, and the lock file names each need of f
# once. A path dependency means the lock file never answers
# alone, so every build resolves again and compares the graph with the
# lock file. A build after the lock file was changed by hand, in the
# name of a need, in the number of needs or in the digest of a module,
# writes the lock file of the graph again.
foreach(package e f g)
    set(project "${WORK}/${package}")
    file(WRITE "${project}/anti.toml"
         "[package]\n"
         "name = \"com.example.${package}\"\n"
         "version = \"1.0.0\"\n")
    file(WRITE "${project}/src/com/example/${package}.anti"
         "//! A package of the resolution test.\n"
         "\n"
         "/// One.\n"
         "pub fn one() -> int\n"
         "{\n"
         "\treturn 1;\n"
         "}\n")
    run_anti("${project}" build)
    set(library_${package}
        "${project}/dist/${HOST}/dev/com/example/${package}.antl")
    file(SHA256 "${library_${package}}" digest_${package})
endforeach()
set(e "")
entry(e e 1.0.0 "{ name = \"com.example.g\", version = \"1.0.0\" }")
publish(e "${e}" 1.0.0)
set(f "")
entry(f f 1.0.0 "{ name = \"com.example.e\", version = \"1.0.0\" }, { name = \"com.example.g\", version = \"1.0.0\" }, { name = \"com.example.g\", version = \"1.0.0\" }, { name = \"com.example.g\" }")
publish(f "${f}" 1.0.0)
set(g "")
entry(g g 1.0.0 "")
publish(g "${g}" 1.0.0)
set(project "${WORK}/diamond")
file(WRITE "${project}/anti.toml"
     "[package]\n"
     "name = \"com.example.diamond\"\n"
     "version = \"0.1.0\"\n"
     "\n"
     "[repositories]\n"
     "local = \"${repo_url}\"\n"
     "\n"
     "[dependencies]\n"
     "\"com.example.e\" = { version = \"1.0.0\", repo = \"local\" }\n"
     "\"com.example.f\" = { version = \"1.0.0\", repo = \"local\" }\n"
     "\"com.example.c\" = { path = \"../c\" }\n")
file(WRITE "${project}/src/com/example/diamond.anti"
     "//! A program over e, f, the g they both need and c of a path.\n"
     "\n"
     "import com.example.c;\n"
     "import com.example.e;\n"
     "import com.example.f;\n"
     "\n"
     "fn main() -> int\n"
     "{\n"
     "\treturn c.one() + e.one() + f.one();\n"
     "}\n")
run_anti("${project}" build)
program_expect("the diamond" COMMAND "${project}/dist/${HOST}/dev/diamond"
               STATUS 3 OUT "")
file(READ "${project}/anti.lock" written)
string(REGEX MATCHALL "name = \"com.example.g\"" g_entries "${written}")
string(REGEX MATCHALL "needs = \\[\"com.example.g\"\\]" g_needs "${written}")
string(REGEX MATCHALL
       "needs = \\[\"com.example.e\", \"com.example.g\"\\]\n"
       f_needs "${written}")
list(LENGTH g_entries g_count)
list(LENGTH g_needs needs_count)
list(LENGTH f_needs f_count)
if(NOT g_count EQUAL 1 OR NOT needs_count EQUAL 1 OR NOT f_count EQUAL 1)
    message(FATAL_ERROR "the lock file of the diamond names g ${g_count} "
                        "times, e alone needs it ${needs_count} times and "
                        "f needs e and g ${f_count} times:\n${written}")
endif()
# The second build finds the same graph, so the lock file stays as it was.
run_anti("${project}" build)
file(READ "${project}/anti.lock" again)
if(NOT again STREQUAL written)
    message(FATAL_ERROR "the second build changed the lock file:\n${again}")
endif()
# Each change by hand is undone by the next build.
function(changed_lock what from to)
    file(READ "${project}/anti.lock" lock)
    string(FIND "${lock}" "${from}" at)
    if(at LESS 0)
        message(FATAL_ERROR "the lock file holds no ${from}:\n${lock}")
    endif()
    string(LENGTH "${from}" length)
    string(SUBSTRING "${lock}" 0 ${at} head)
    math(EXPR rest "${at} + ${length}")
    string(SUBSTRING "${lock}" ${rest} -1 tail)
    file(WRITE "${project}/anti.lock" "${head}${to}${tail}")
    run_anti("${project}" build)
    file(READ "${project}/anti.lock" lock)
    if(NOT lock STREQUAL written)
        message(FATAL_ERROR "the build after ${what} wrote\n${lock}")
    endif()
endfunction()
changed_lock("a need of another name" "needs = [\"com.example.g\"]"
             "needs = [\"com.example.c\"]")
changed_lock("a need taken out" "needs = [\"com.example.g\"]" "needs = []")
changed_lock("another digest" "${digest_g}" "${digest_e}")
changed_lock("a need added" "needs = [\"com.example.g\"]"
             "needs = [\"com.example.g\", \"com.example.c\"]")
changed_lock("a module added"
             "{ path = \"com.example.g\", sha256 = \"${digest_g}\" },"
             "{ path = \"com.example.g\", sha256 = \"${digest_g}\" },\n    { path = \"com.example.h\", sha256 = \"${digest_g}\" },")
changed_lock("a package named twice"
             "[[package]]\nname = \"com.example.g\"\n"
             "[[package]]\nname = \"com.example.g\"\nversion = \"1.0.0\"\n\n[[package]]\nname = \"com.example.g\"\n")

# The fourth part, in a repository of its own: the project needs a 1.x
# and h 1.x, h needs k and k needs a =1.0.0. The first round picks a
# 1.1.0, which needs c =2.0.0 and d, and h. The second picks c 2.0.0, d,
# which needs c as well, and k. The third takes a back, and with it c and
# d. d gives c to the work a second time, which finds c out of the graph
# already. The lock file names a 1.0.0 and c 1.0.0, and no d.
foreach(package h k)
    set(project "${WORK}/${package}")
    file(WRITE "${project}/anti.toml"
         "[package]\n"
         "name = \"com.example.${package}\"\n"
         "version = \"1.0.0\"\n")
    file(WRITE "${project}/src/com/example/${package}.anti"
         "//! A package of the resolution test.\n"
         "\n"
         "/// One.\n"
         "pub fn one() -> int\n"
         "{\n"
         "\treturn 1;\n"
         "}\n")
    run_anti("${project}" build)
    set(library_${package}
        "${project}/dist/${HOST}/dev/com/example/${package}.antl")
    file(SHA256 "${library_${package}}" digest_${package})
endforeach()
set(repo "${WORK}/repo-back")
set(a "")
entry(a a 1.0.0 "{ name = \"com.example.c\", version = \"=1.0.0\" }")
entry(a a 1.1.0 "{ name = \"com.example.c\", version = \"=2.0.0\" }, { name = \"com.example.d\", version = \"1.0.0\" }")
publish(a "${a}" 1.0.0 1.1.0)
set(c "")
entry(c c 1.0.0 "")
entry(c c 2.0.0 "")
publish(c "${c}" 1.0.0 2.0.0)
set(d "")
entry(d d 1.0.0 "{ name = \"com.example.c\", version = \">=1.0.0\" }")
publish(d "${d}" 1.0.0)
set(h "")
entry(h h 1.0.0 "{ name = \"com.example.k\", version = \"1.0.0\" }")
publish(h "${h}" 1.0.0)
set(k "")
entry(k k 1.0.0 "{ name = \"com.example.a\", version = \"=1.0.0\" }")
publish(k "${k}" 1.0.0)
set(project "${WORK}/back")
file_url("${repo}" back_url)
file(WRITE "${project}/anti.toml"
     "[package]\n"
     "name = \"com.example.back\"\n"
     "version = \"0.1.0\"\n"
     "\n"
     "[repositories]\n"
     "local = \"${back_url}\"\n"
     "\n"
     "[dependencies]\n"
     "\"com.example.a\" = { version = \"1.0.0\", repo = \"local\" }\n"
     "\"com.example.h\" = { version = \"1.0.0\", repo = \"local\" }\n")
file(WRITE "${project}/src/com/example/back.anti"
     "//! A program over a, h and the packages they need.\n"
     "\n"
     "import com.example.a;\n"
     "import com.example.h;\n"
     "\n"
     "fn main() -> int\n"
     "{\n"
     "\treturn a.one() + h.one();\n"
     "}\n")
run_anti("${project}" build)
program_expect("the program of the fourth part"
               COMMAND "${project}/dist/${HOST}/dev/back" STATUS 2 OUT "")
file(READ "${project}/anti.lock" lock)
foreach(pick "a\"\nversion = \"1.0.0" "c\"\nversion = \"1.0.0"
        "h\"\nversion = \"1.0.0" "k\"\nversion = \"1.0.0")
    if(NOT lock MATCHES "name = \"com.example.${pick}\"")
        message(FATAL_ERROR "the lock file of the fourth part names no "
                            "com.example.${pick}:\n${lock}")
    endif()
endforeach()
if(lock MATCHES "com.example.d")
    message(FATAL_ERROR "the lock file of the fourth part keeps "
                        "com.example.d, which only the dropped pick of a "
                        "needs:\n${lock}")
endif()
