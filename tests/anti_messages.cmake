# The messages of the commands of `anti` that have no directory under
# tests/: `anti new`, `anti license` and `anti sdk`. One case each.
# tests/run_anti_messages.cmake states the form of a case.

# anti new.
anti_message(new_one_segment
    ARGS new app
    ERR "anti: `app` is one segment, and a package name is a module path of at least two, as com.example.app")
anti_message(new_there_already
    FILES app/notes.txt "The directory of an older project.\n"
    ARGS new com.example.app
    ERR "anti: app is there already")

# anti license.
anti_message(license_runtime_unknown
    ENV "XDG_DATA_HOME=${WORK}/data"
    ARGS license
    ERR "anti: the system does not say where the runtime archive is, so license needs --runtime")
anti_message(license_no_text
    DIRECTORIES archive/licenses
    ARGS license --runtime archive
    ERR "anti: archive/licenses holds no licence text, so archive is no runtime archive")
anti_message(license_from_no_notice
    FILES program "A file that no link of antic wrote.\n"
    ARGS license --from program --runtime "${RUNTIME}"
    ERR "anti: program carries no licence notice")
anti_message(license_from_absent
    ARGS license --from none --runtime "${RUNTIME}"
    ERR "anti: cannot read none")
# A notice ends at its end marker, so the bytes of a program that lacks
# the marker hold none.
set(license_id "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
set(license_head "MZ\nANTI_LICENSES_BEGIN\nbuild ${license_id}\npackage com.example.app 1.0 MIT\n")
anti_message(license_from_notice_without_its_end
    FILES program "${license_head}text for com.example.app\nANTI_LICENSES_EN"
    ARGS license --from program --runtime "${RUNTIME}"
    ERR "anti: program carries no licence notice")
# The last line of a notice and of the record of sources may lack its
# newline. The record names `musl` with its URL and `mimalloc` without
# one, and no `other`.
set(license_sources "# The record.\nmusl 1.2.5 https://musl.libc.org/releases/musl-1.2.5.tar.gz\nmimalloc 2.1.7")
anti_message(license_from_notice_without_a_last_newline
    STATUS 0
    FILES program "${license_head}text for musl other mimallocANTI_LICENSES_END\n"
          archive/licenses/sources.txt "${license_sources}"
    ARGS license --from program --runtime archive
    OUT "package com.example.app 1.0 MIT"
        "text for musl other mimallocsource musl 1.2.5 https://musl.libc.org/releases/musl-1.2.5.tar.gz"
        "source mimalloc 2.1.7")
# The plain form takes the version of a component from the record, where
# the line of `mimalloc` ends the file and holds no URL.
anti_message(license_sources_without_a_last_newline
    STATUS 0
    FILES archive/LICENSE "The licence of Anti.\n"
          archive/licenses/anti_rt.txt "The licence of the runtime.\n"
          archive/licenses/mimalloc.txt "The licence of mimalloc.\n"
          archive/licenses/sources.txt "${license_sources}"
    ARGS license --runtime archive
    OUT "package antic ${VERSION} MIT"
        "package anti ${VERSION} MIT"
        "package anti.rt ${VERSION} 0BSD"
        "package mimalloc 2.1.7 MIT"
        "text for antic anti"
        "The licence of Anti."
        "text for anti.rt"
        "The licence of the runtime."
        "text for mimalloc"
        "The licence of mimalloc."
        "source mimalloc 2.1.7")
anti_message(license_from_archive_no_archive
    FILES library.a "A text where the archive belongs.\n"
    ARGS license --from-archive library.a
    ERR "anti: library.a is no archive")
anti_message(license_from_archive_absent
    ARGS license --from-archive none.a
    ERR "anti: cannot read none.a")
# The header of a member holds its size as digits. By R187 of "Rules of
# 2026-10-13" in docs/decisions.md an archive whose member holds spaces
# there is damaged, at the header of that member.
anti_message(license_from_archive_member_without_a_size
    FILES library.a "!<arch>\nmember.o/       0           0     0     644               `\n"
    ARGS license --from-archive library.a
    ERR "anti: library.a is damaged at byte 8")
anti_message(license_from_archive_no_header
    FILES library.a "!<arch>\n"
    ARGS license --from-archive library.a
    ERR "anti: library.a carries no package header, so it is no static library of `--lib static`")
anti_message(license_project_all_targets STATUS 2
    ARGS license --project --target all
    ERR "anti: license --project prints the notice of one target, so it takes --target <os>-<cpu> and not all")
anti_message(license_project_static_library STATUS 2
    ARGS license --project --lib static
    ERR "anti: the build of a static library prints the line that links it, so license --project --lib static takes --notice")

# anti sdk import.
function(pack_bundle)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cJf
                            "${PROJECT}/bundle.tar.xz" notes.txt
                    WORKING_DIRECTORY "${PROJECT}/stubs"
                    RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the bundle was not written")
    endif()
endfunction()
anti_message(sdk_import_sysroot_unknown
    FILES bundle.tar.xz "A text where the bundle belongs.\n"
    ENV "XDG_DATA_HOME=${WORK}/data"
    ARGS sdk import bundle.tar.xz
    ERR "anti: the system does not say where anti is, so pass --sysroot")
# R156 of "Rules of 2026-10-13" in docs/decisions.md: the import names
# the path that is a file where the directory of a target belongs, and
# exits with 1.
anti_message(sdk_import_sysroot_is_a_file
    FILES bundle.tar.xz "A text where the bundle belongs.\n"
          sysroot "A file where the directory of the sysroots belongs.\n"
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: sysroot is a file where a directory belongs")
# tar writes its own lines on the bundle before the one of anti.
anti_message(sdk_import_no_bundle
    FILES bundle.tar.xz "A text where the bundle belongs.\n"
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR_MATCH "(.*\n)*anti: tar could not unpack bundle\\.tar\\.xz\n")
anti_message(sdk_import_no_version
    FILES stubs/notes.txt "No stub and no version.\n"
    SETUP pack_bundle
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: bundle.tar.xz holds no sdk-version, so anti sdk export did not write it")

# anti sdk export, which reads the SDK of a Mac.
if(CMAKE_HOST_APPLE)
    set(settings "{\"Version\": \"14.0\"}\n")
    anti_message(sdk_export_no_settings
        FILES MacOSX.sdk/notes.txt "No settings.\n"
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: MacOSX.sdk holds no SDKSettings.json with a Version, so it is no MacOSX.sdk")
    anti_message(sdk_export_version
        FILES MacOSX.sdk/SDKSettings.json "{\"Version\": \"../14\"}\n"
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: MacOSX.sdk names the Version ../14, which is no version")
    anti_message(sdk_export_version_with_another_sign
        FILES MacOSX.sdk/SDKSettings.json "{\"Version\": \"15_2\"}\n"
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: MacOSX.sdk names the Version 15_2, which is no version")
    anti_message(sdk_export_out_is_a_file
        FILES MacOSX.sdk/SDKSettings.json "${settings}"
              out "A file where the directory of the bundle belongs.\n"
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot make out/.apple-sdk-14.0")
    anti_message(sdk_export_no_stubs
        FILES MacOSX.sdk/SDKSettings.json "${settings}"
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot copy the stubs of MacOSX.sdk/usr/lib")
    # R163 of "Rules of 2026-10-13" in docs/decisions.md: a program anti
    # cannot start is reported with the name anti.
    anti_message(sdk_export_no_tar
        FILES MacOSX.sdk/SDKSettings.json "${settings}"
        DIRECTORIES MacOSX.sdk/usr/lib MacOSX.sdk/System/Library/Frameworks
                    empty
        ENV "PATH=${PROJECT}/empty"
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot run tar: No such file or directory"
            "anti: tar could not write out/apple-sdk-14.0.tar.xz")
else()
    anti_message(sdk_export_no_mac
        ARGS sdk export -o out
        ERR "anti: sdk export runs on a Mac, which holds Apple's SDK. Run it on a Mac you own and anti sdk import here.")
endif()
