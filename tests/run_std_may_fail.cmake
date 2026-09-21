# No failing function of the standard library writes the out-pointer
# convention by hand, since the standard library uses `may fail`
# throughout. A binding keeps the hand-written form, because it refuses
# `may fail`. A `construct` that fails is written `may fail` like any
# other function. Run with cmake -P and STD, the directory that holds
# the modules.

file(GLOB sources "${STD}/*.anti")
if(NOT sources)
    message(FATAL_ERROR "no module in ${STD}")
endif()
set(offenders "")
foreach(source IN LISTS sources)
    file(READ "${source}" content)
    string(REGEX MATCHALL
        "(extern )?fn [A-Za-z_0-9]+\\([^)]*\\)[ \t\r\n]*-> \\?\\*[A-Za-z_.]*Error"
        found "${content}")
    foreach(declaration IN LISTS found)
        if(declaration MATCHES "^extern ")
            continue()
        endif()
        string(REGEX MATCH "^fn [A-Za-z_0-9]+" name "${declaration}")
        get_filename_component(file "${source}" NAME)
        string(APPEND offenders "\n  ${file}: ${name}")
    endforeach()
endforeach()
if(offenders)
    message(FATAL_ERROR
        "written as -> ?*Error by hand rather than may fail:${offenders}")
endif()
