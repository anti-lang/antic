# No function of the standard library or of the test programs writes
# the out-pointer convention by hand, since both use `may fail`
# throughout. A function written `-> ?*Error` without `may fail` is an
# ordinary function whose error is a value, and one written so outside a
# binding is most likely a failing function in the old form. A binding
# keeps the form, because it refuses `may fail`. A `construct` that fails
# is written `may fail` like any other function. Run with cmake -P and ROOT, the directory whose `.anti`
# files are read at every depth. SKIP lists the declarations that write
# the form on purpose, each as `<path under ROOT>:<function>`, separated
# by commas.

string(REPLACE "," ";" skipped "${SKIP}")
file(GLOB_RECURSE sources "${ROOT}/*.anti")
if(NOT sources)
    message(FATAL_ERROR "no module in ${ROOT}")
endif()
set(offenders "")
foreach(source IN LISTS sources)
    file(READ "${source}" content)
    string(REGEX MATCHALL
        "(extern )?fn [A-Za-z_0-9]+\\([^)]*\\)[ \t\r\n]*-> \\?\\*[A-Za-z_.]*Error"
        found "${content}")
    file(RELATIVE_PATH file "${ROOT}" "${source}")
    foreach(declaration IN LISTS found)
        if(declaration MATCHES "^extern ")
            continue()
        endif()
        string(REGEX MATCH "^fn ([A-Za-z_0-9]+)" name "${declaration}")
        if("${file}:${CMAKE_MATCH_1}" IN_LIST skipped)
            continue()
        endif()
        string(APPEND offenders "\n  ${file}: ${name}")
    endforeach()
endforeach()
if(offenders)
    message(FATAL_ERROR
        "written as -> ?*Error by hand rather than may fail:${offenders}")
endif()
