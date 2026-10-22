# The messages of the commands of `anti` that have no directory under
# tests/: `anti new`, `anti license` and `anti sdk`. One case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

# anti new.
anti_message(new_one_segment
    ARGS new app
    ERR "anti: `app` is one segment, and a package name is a module path of at least two, as com.example.app")
# Defect 7 of the anti-tool audit: anti new counted the dots of its name
# alone, so it wrote a project that no build takes, and a name that holds
# a path wrote its project at that path.
anti_message(new_not_a_package_name
    ARGS new com.My-App
    ERR "anti: `com.My-App` is no package name, which is lowercase segments of letters, digits and `_` joined by dots, as com.example.app")
anti_message(new_name_with_a_path
    ARGS new a.b/c
    ERR "anti: `a.b/c` is no package name, which is lowercase segments of letters, digits and `_` joined by dots, as com.example.app")
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
# one, and no `other`. By R189 of "Rules of 2026-10-13" in
# docs/decisions.md the last line of the notice gains its newline before
# the lines of the sources.
set(license_sources "# The record.\nmusl 1.2.5 https://musl.libc.org/releases/musl-1.2.5.tar.gz\nmimalloc 2.1.7")
anti_message(license_from_notice_without_a_last_newline
    STATUS 0
    FILES program "${license_head}text for musl other mimallocANTI_LICENSES_END\n"
          archive/licenses/sources.txt "${license_sources}"
    ARGS license --from program --runtime archive
    OUT "package com.example.app 1.0 MIT"
        "text for musl other mimalloc"
        "source musl 1.2.5 https://musl.libc.org/releases/musl-1.2.5.tar.gz"
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
# R188 of "Rules of 2026-10-13" in docs/decisions.md: an archive without
# the licence of the runtime is named by anti, which ends with 1.
anti_message(license_runtime_without_its_text
    FILES archive/LICENSE "The licence of Anti.\n"
          archive/licenses/mimalloc.txt "The licence of mimalloc.\n"
          archive/licenses/sources.txt "${license_sources}"
    ARGS license --runtime archive
    ERR "anti: cannot read archive/licenses/anti_rt.txt")
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
# Without --sysroot the import takes the sysroot of the runtime archive of
# the user's data directory, default_sysroot of main.c.
anti_message(sdk_import_sysroot_of_the_data_directory
    FILES stubs/notes.txt "No stub and no version.\n"
    DIRECTORIES data/anti/lib
    ENV "XDG_DATA_HOME=${PROJECT}/data" "LOCALAPPDATA=${PROJECT}/data"
    SETUP pack_bundle
    ARGS sdk import bundle.tar.xz
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
    # The Version of the top object names the SDK, and a member of the
    # same name in a nested object that stands first does not.
    anti_message(sdk_export_version_of_the_top_object
        FILES MacOSX.sdk/SDKSettings.json
              "{\"DefaultProperties\": {\"Version\": \"../1\"}, \"Version\": \"14.0\"}\n"
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot copy the stubs of MacOSX.sdk/usr/lib")
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
    # Without --sdk the export lists the SDKs of the Command Line Tools,
    # apple_clt_sdk of src/antic/applesdk.c. The listing that fails is one
    # that holds none.
    anti_message(fault_sdk_export_listing_unread
        FAULT open:1:EACCES
        ARGS sdk export -o out
        ERR "anti: /Library/Developer/CommandLineTools/SDKs holds no SDK. Run xcode-select --install, or name one with --sdk")
else()
    anti_message(sdk_export_no_mac
        ARGS sdk export -o out
        ERR "anti: sdk export runs on a Mac, which holds Apple's SDK. Run it on a Mac you own and anti sdk import here.")
endif()

# Fault injection: each case makes one call of the system fail in
# anti_faults, through the variable ANTI_FAULT of src/rt/fault.h. A count
# names the call among those of its kind in the run.

# anti new, project.c. T153: the directory of the project, which anti new
# leaves without a message.
anti_message(fault_new_directory_not_made
    FAULT mkdir:1:EACCES
    ARGS new com.example.app
    ERR "anti: cannot make app")
anti_message(fault_new_manifest_not_written
    FAULT open:1:EACCES
    ARGS new com.example.app
    ERR "anti: cannot write app/anti.toml")
anti_message(fault_new_module_not_written
    FAULT open:2:EACCES
    ARGS new com.example.app
    ERR "anti: cannot write app/src/com/example/app.anti")
# T153: the directory `test`. Each path is made from the top down, so the
# directory of the module takes the project and its three directories,
# and `test` the project and itself: the seventh mkdir.
anti_message(fault_new_test_directory_not_made
    FAULT mkdir:7:EACCES
    ARGS new com.example.app
    ERR "anti: cannot make app/test")
# anti license of a runtime archive opens the listing of licenses/, the
# record of the sources and the licence of antic and anti, in that order.
anti_message(fault_license_listing_unread
    FAULT open:1:EACCES
    ARGS license --runtime "${RUNTIME}"
    ERR "anti: cannot read ${RUNTIME}/licenses")
anti_message(fault_license_text_unread
    FAULT open:3:EACCES
    ARGS license --runtime "${RUNTIME}"
    ERR "anti: cannot read ${RUNTIME}/LICENSE")
# anti sdk import of a bundle that holds sdk-version, sdk.c. The digest
# reads the bundle first, and each target of the Mac takes a directory, a
# part that tar unpacks into, the rename of the part and its digest.
function(pack_versioned_bundle)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cJf
                            "${PROJECT}/bundle.tar.xz" sdk-version
                    WORKING_DIRECTORY "${PROJECT}/stubs"
                    RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the bundle was not written")
    endif()
endfunction()
anti_message(fault_sdk_import_unread
    FILES stubs/sdk-version "14.0\n"
    SETUP pack_versioned_bundle
    FAULT open:1:EACCES
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: cannot read bundle.tar.xz")
anti_message(fault_sdk_import_directory_not_made
    FILES stubs/sdk-version "14.0\n"
    SETUP pack_versioned_bundle
    FAULT mkdir:1:EACCES
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: cannot make sysroot/macos-arm64")
anti_message(fault_sdk_import_part_not_made
    FILES stubs/sdk-version "14.0\n"
    SETUP pack_versioned_bundle
    FAULT mkdir:5:EACCES
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: cannot make sysroot/macos-arm64/sdk.part")
anti_message(fault_sdk_import_tar_not_started
    FILES stubs/sdk-version "14.0\n"
    SETUP pack_versioned_bundle
    FAULT run:1:EACCES
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: cannot run tar: Permission denied"
        "anti: tar could not unpack bundle.tar.xz")
anti_message(fault_sdk_import_not_replaced
    FILES stubs/sdk-version "14.0\n"
    SETUP pack_versioned_bundle
    FAULT rename:1:EACCES
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: cannot replace sysroot/macos-arm64/sdk")
anti_message(fault_sdk_import_digest_not_written
    FILES stubs/sdk-version "14.0\n"
    SETUP pack_versioned_bundle
    FAULT open:2:EACCES
    ARGS sdk import bundle.tar.xz --sysroot sysroot
    ERR "anti: cannot write sysroot/macos-arm64/sdk/digest")
# anti sdk export of an SDK with one stub in usr/lib, sdk.c. The run opens
# SDKSettings.json, the directory usr/lib, the stub, its copy and the
# directory System/Library/Frameworks, and writes the copy and then
# sdk-version. The third stat reads the kind of the stub.
if(CMAKE_HOST_APPLE)
    set(sdk_files MacOSX.sdk/SDKSettings.json "{\"Version\": \"14.0\"}\n"
                  MacOSX.sdk/usr/lib/libSystem.tbd "A stub.\n")
    anti_message(fault_sdk_export_stub_unread
        FILES ${sdk_files}
        DIRECTORIES MacOSX.sdk/System/Library/Frameworks
        FAULT open:3:EACCES
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot copy the stubs of MacOSX.sdk/usr/lib")
    anti_message(fault_sdk_export_stub_not_written
        FILES ${sdk_files}
        DIRECTORIES MacOSX.sdk/System/Library/Frameworks
        FAULT write:1:ENOSPC
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot copy the stubs of MacOSX.sdk/usr/lib")
    anti_message(fault_sdk_export_frameworks_unread
        FILES ${sdk_files}
        DIRECTORIES MacOSX.sdk/System/Library/Frameworks
        FAULT open:5:EACCES
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot copy the stubs of MacOSX.sdk/System/Library/Frameworks")
    anti_message(fault_sdk_export_version_not_written
        FILES ${sdk_files}
        DIRECTORIES MacOSX.sdk/System/Library/Frameworks
        FAULT write:2:ENOSPC
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot write out/.apple-sdk-14.0/sdk-version")
    # T155: a stub whose kind the system refuses to give, which copy_stubs
    # leaves out of the bundle.
    anti_message(fault_sdk_export_stub_kind_unread
        FILES ${sdk_files}
        DIRECTORIES MacOSX.sdk/System/Library/Frameworks
        FAULT stat:3:EACCES
        ARGS sdk export --sdk MacOSX.sdk -o out
        ERR "anti: cannot copy the stubs of MacOSX.sdk/usr/lib")
endif()
