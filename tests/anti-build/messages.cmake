# The messages of `anti build` and of `anti run`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

set(manifest_package "[package]\nname = \"com.example.app\"\nversion = \"0.1.0\"\n")

# The manifest.
anti_message(manifest_missing
    ARGS build
    ERR "anti: ./anti.toml: no manifest here, so there is no project. `anti new <name>` writes one")
anti_message(manifest_no_toml
    FILES anti.toml "[package\n"
    ARGS build
    ERR "anti: ./anti.toml is no TOML that anti reads")
anti_message(manifest_no_name
    FILES anti.toml "[package]\nversion = \"0.1.0\"\n"
    ARGS build
    ERR "anti: ./anti.toml: [package] names no `name`, the root module path of the package")
anti_message(manifest_version
    FILES anti.toml "[package]\nname = \"com.example.app\"\nversion = \"1.x\"\n"
    ARGS build
    ERR "anti: ./anti.toml: [package] version `1.x` is no version of one to three parts of digits, as `1.2.0`")
anti_message(dependency_no_table
    FILES anti.toml "${manifest_package}\n[dependencies]\nunits = \"1.2.0\"\n"
    ARGS build
    ERR "anti: ./anti.toml: the dependency units needs a table, as { version = \"1.0.0\" }")
anti_message(dependency_field
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\", branch = \"main\" }\n"
    ARGS build
    ERR "anti: ./anti.toml: a dependency has no branch field")
anti_message(dependency_repository_and_path
    FILES anti.toml "${manifest_package}\n[repositories]\nlocal = \"https://example.com/repo\"\n\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\", repo = \"local\", path = \"../units\" }\n"
    ARGS build
    ERR "anti: ./anti.toml: the dependency com.example.units names a repository and a path")
anti_message(dependency_no_version
    FILES anti.toml "${manifest_package}\n[repositories]\nlocal = \"https://example.com/repo\"\n\n[dependencies]\n\"com.example.units\" = { repo = \"local\" }\n"
    ARGS build
    ERR "anti: ./anti.toml: the dependency com.example.units names no version")
anti_message(dependency_repository_unknown
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\", repo = \"local\" }\n"
    ARGS build
    ERR "anti: ./anti.toml: the dependency com.example.units names the repository local, which [repositories] does not")

# The build of a project. The module of each is the smallest program.
set(app_source "src/com/example/app.anti")
set(app_text "//! The smallest program.\n\nfn main() -> int\n{\n\treturn 0;\n}\n")
set(library_text "//! The smallest library.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
set(tools --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")

anti_message(antic_version
    FILES anti.toml "[package]\nname = \"com.example.app\"\nversion = \"0.1.0\"\nantic = \"99.0.0\"\n"
          ${app_source} "${app_text}"
    ARGS build ${tools}
    ERR "anti: ./anti.toml asks for antic 99.0.0 and this one is ${VERSION}")
anti_message(runtime_unknown
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
    ENV "XDG_DATA_HOME=${WORK}/data"
    ARGS build
    ERR "anti: the system does not say where the runtime archive is, so the build needs --runtime")
anti_message(no_module
    FILES anti.toml "${manifest_package}" src/notes.txt "No module.\n"
    ARGS build ${tools}
    ERR "anti: ./src holds no .anti file, so this project has no module")
anti_message(target_unknown
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
    ARGS build --target plan9-x86_64 ${tools}
    ERR "anti: plan9-x86_64 is no target of Anti")
anti_message(targets_seven
    FILES anti.toml "${manifest_package}\n[targets]\ndefault = [\"macos-arm64\", \"macos-x86_64\", \"linux-arm64\", \"linux-x86_64\", \"windows-arm64\", \"windows-x86_64\", \"macos-arm64\"]\n"
          ${app_source} "${app_text}"
    ARGS build ${tools}
    ERR "anti: anti.toml: [targets] names more than the six targets of Anti")
anti_message(targets_unknown
    FILES anti.toml "${manifest_package}\n[targets]\ndefault = [\"plan9-x86_64\"]\n"
          ${app_source} "${app_text}"
    ARGS build ${tools}
    ERR "anti: anti.toml: [targets] names plan9-x86_64, which is no target of Anti")
anti_message(cpu_level
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
    ARGS build --target linux-x86_64 --cpu v9 ${tools}
    ERR "anti: v9 is no processor level of linux-x86_64")
# R158 of "Rules of 2026-10-13" in docs/decisions.md: --target all checks
# every target against the level before it builds any. The source does
# not compile, so a build of linux-x86_64 would add the message of antic.
anti_message(cpu_level_of_all_targets
    FILES anti.toml "${manifest_package}"
          ${app_source} "fn main() -> int\n{\n\treturn missing;\n}\n"
    ARGS build --target all --cpu v1 ${tools}
    ERR "anti: v1 is no processor level of linux-arm64")
# R156 of "Rules of 2026-10-13" in docs/decisions.md: the build names
# the path that is a file where its directory belongs, and exits with 1.
anti_message(dist_is_a_file
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
          dist "A file where the directory of the programs belongs.\n"
    ARGS build ${tools}
    ERR "anti: ./dist is a file where a directory belongs")
anti_message(program_is_a_directory
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
          dist/${HOST}/dev/app${EXE}/notes.txt "A directory where the program belongs.\n"
    ARGS build ${tools}
    ERR "anti: cannot write ./dist/${HOST}/dev/app${EXE}")
# R160 of "Rules of 2026-10-13" in docs/decisions.md: run of a library
# project refuses before it builds, and ends with 2. The build would
# write dist/.
anti_message(run_a_library STATUS 2
    FILES anti.toml "[package]\nname = \"com.example.one\"\nversion = \"0.1.0\"\n"
          src/com/example/one.anti "${library_text}"
          dist "A file where the build would make a directory.\n"
    ARGS run ${tools}
    ERR "anti: com.example.one is a library project, which has no `main` to run")
# run builds the default targets of the manifest, and one that is not this
# host writes no program to run here.
if(HOST STREQUAL "linux-x86_64")
    set(other_target "linux-arm64")
else()
    set(other_target "linux-x86_64")
endif()
anti_message(run_without_the_host
    FILES anti.toml "${manifest_package}\n[targets]\ndefault = [\"${other_target}\"]\n"
          ${app_source} "${app_text}"
    ARGS run ${tools}
    ERR "anti: ./dist/${HOST}/dev/app${EXE} was not built, so there is nothing to run")
# R159 of "Rules of 2026-10-13" in docs/decisions.md: run builds a program
# for this machine, and refuses an option of build that makes none.
foreach(case "target|--target|linux-x86_64" "lib|--lib|static"
        "bundle_runtime|--bundle-runtime" "soname|--soname"
        "strip_docs|--strip-docs" "llvm_ar|--llvm-ar|llvm-ar")
    string(REPLACE "|" ";" parts "${case}")
    list(POP_FRONT parts name)
    list(GET parts 0 option)
    anti_message(run_${name} STATUS 2
        ARGS run ${parts}
        ERR "anti: run builds a program for this machine, so it takes no ${option}")
endforeach()
anti_message(path_cycle
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.one\" = { path = \"../one\" }\n"
          ${app_source} "${app_text}"
          ../one/anti.toml "[package]\nname = \"com.example.one\"\nversion = \"0.1.0\"\n\n[dependencies]\n\"com.example.app\" = { path = \"../project\" }\n"
          ../one/src/com/example/one.anti "${library_text}"
    ARGS build ${tools}
    ERR "anti: the path dependencies form a cycle: com.example.app -> com.example.one -> com.example.app")
# R165 of "Rules of 2026-10-13" in docs/decisions.md: a cycle is refused at
# any depth. Nine projects stand in a chain below this one, and the last
# names the one before it, nine levels down.
function(write_chain)
    foreach(level RANGE 1 9)
        if(level EQUAL 9)
            set(next 8)
        else()
            math(EXPR next "${level} + 1")
        endif()
        file(WRITE "${PROJECT}/../p${level}/anti.toml"
             "[package]\nname = \"com.example.p${level}\"\nversion = \"0.1.0\"\n\n[dependencies]\n\"com.example.p${next}\" = { path = \"../p${next}\" }\n")
        file(WRITE "${PROJECT}/../p${level}/src/com/example/p${level}.anti"
             "${library_text}")
    endforeach()
endfunction()
anti_message(path_cycle_nine_deep
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.p1\" = { path = \"../p1\" }\n"
          ${app_source} "${app_text}"
    SETUP write_chain
    ARGS build ${tools}
    ERR "anti: the path dependencies form a cycle: com.example.p8 -> com.example.p9 -> com.example.p8")

# The dependencies. The repository of a case is the directory WORK/repo,
# named by its file:// URL, and the cache of the user is WORK/cache.
file_url("${WORK}/repo" repo_url)
set(units_digest "5b0c2d1e7a94f3c6b8d0e1f2a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6")
set(consumer_manifest "${manifest_package}\n[repositories]\nlocal = \"${repo_url}\"\n\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\", repo = \"local\" }\n")
set(units_index "../repo/com.example.units/index.toml")
set(index_head "name = \"com.example.units\"\nrevision = 1\n\n[[version]]\n")
# The index of a repository in the cache: index/<digest of the URL>/<name>.
string(SHA256 repo_digest "${repo_url}")
set(cached_index "${USER_CACHE}/index/${repo_digest}/com.example.units/index.toml")
set(units_module "{ path = \"com.example.units\", sha256 = \"${units_digest}\" }")
# What the resolver adds where the named repository gave no version.
set(no_version "anti: ${repo_url} has no version of com.example.units that satisfies every constraint on it")

anti_message(index_no_toml
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "name = \n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} is no index that anti reads"
        "${no_version}")
anti_message(index_version
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.x\"\nmodules = [${units_module}]\n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} names the version 1.x of com.example.units, which is no version"
        "${no_version}")
anti_message(index_no_module
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = []\n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} names no module of com.example.units 1.2.0"
        "${no_version}")
anti_message(index_module_path
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = [{ path = \"com/example\", sha256 = \"${units_digest}\" }]\n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} names the module com/example, which is no module path"
        "${no_version}")
anti_message(index_digest
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = [{ path = \"com.example.units\", sha256 = \"5b0c\" }]\n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} names the digest 5b0c of com.example.units, which is no SHA-256 digest"
        "${no_version}")
anti_message(index_dependency_name
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = [${units_module}]\ndependencies = [{ name = \"com/example\", version = \"1.0.0\" }]\n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} names the dependency com/example, which is no package name"
        "${no_version}")
anti_message(index_dependency_constraint
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = [${units_module}]\ndependencies = [{ name = \"com.example.other\", version = \"1.0/x\" }]\n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} names the version 1.0/x of com.example.other, which is no constraint"
        "${no_version}")
anti_message(index_older_alone
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.1.0\"\nmodules = [${units_module}]\n"
    ARGS build ${tools}
    ERR "${no_version}")
# `file://localhost/tmp/repo` names the directory that `file:///tmp/repo`
# names. The index is read, and it holds an older version alone.
string(REPLACE "file://" "file://localhost" localhost_url "${repo_url}")
anti_message(repository_of_localhost
    FILES anti.toml "${manifest_package}\n[repositories]\nlocal = \"${localhost_url}\"\n\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\", repo = \"local\" }\n"
          ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.1.0\"\nmodules = [${units_module}]\n"
    ARGS build ${tools}
    ERR "anti: ${localhost_url} has no version of com.example.units that satisfies every constraint on it")
anti_message(index_is_a_directory
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index}/notes.txt "A directory where the index belongs.\n"
    ARGS build ${tools}
    ERR "anti: ${repo_url}/com.example.units/index.toml: cannot read ${WORK}/repo/com.example.units/index.toml"
        "${no_version}")
anti_message(library_absent
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = [${units_module}]\n"
    ARGS build ${tools}
    ERR "anti: ${repo_url}/com.example.units/1.2.0/com.example.units.antl: the repository holds no such file")
string(SHA256 changed_digest "A library file that changed after its index was written.\n")
anti_message(library_digest
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = [${units_module}]\n"
          ../repo/com.example.units/1.2.0/com.example.units.antl "A library file that changed after its index was written.\n"
    ARGS build ${tools}
    ERR "anti: com.example.units of com.example.units 1.2.0 has the digest ${changed_digest} and the index names ${units_digest}")
anti_message(offline_no_index
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
    ARGS build --offline ${tools}
    ERR "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")

# The dependency names no repository, so every one of the manifest is asked.
file_url("${WORK}/second" second_url)
set(asking_manifest "${manifest_package}\n[repositories]\nlocal = \"${repo_url}\"\nsecond = \"${second_url}\"\n\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\" }\n")
anti_message(repositories_none_holds
    FILES anti.toml "${asking_manifest}" ${app_source} "${app_text}"
    ARGS build ${tools}
    ERR "anti: no repository of the manifest has a version of com.example.units that satisfies every constraint on it")
anti_message(repositories_both_hold
    FILES anti.toml "${asking_manifest}" ${app_source} "${app_text}"
          ${units_index} "${index_head}version = \"1.2.0\"\nmodules = [${units_module}]\n"
          ../second/com.example.units/index.toml "${index_head}version = \"1.2.0\"\nmodules = [${units_module}]\n"
    ARGS build ${tools}
    ERR "anti: ${repo_url} and ${second_url} both hold com.example.units, so the dependency names which one")
anti_message(repositories_one_unread
    FILES anti.toml "${asking_manifest}" ${app_source} "${app_text}"
          ${units_index} "name = \n"
    ARGS build ${tools}
    ERR "anti: ${cached_index} is no index that anti reads"
        "anti: ${repo_url} gave no index of com.example.units that anti reads, so the build cannot tell which repository holds it")

# The URL of a repository.
foreach(case "http|http://example.com/repo|anti: http://example.com/repo/com.example.units/index.toml: http:// names a repository of 127.0.0.1 or localhost alone"
             "scheme|ftp://example.com/repo|anti: ftp://example.com/repo/com.example.units/index.toml: a repository URL is https:// or file://"
             "relative|file://repo|anti: file://repo/com.example.units/index.toml: a file:// URL names an absolute path")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 url)
    list(GET parts 2 line)
    anti_message(repository_url_${name}
        FILES anti.toml "${manifest_package}\n[repositories]\nlocal = \"${url}\"\n\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\", repo = \"local\" }\n"
              ${app_source} "${app_text}"
        ARGS build ${tools}
        ERR "${line}"
            "anti: ${url} has no version of com.example.units that satisfies every constraint on it")
endforeach()
# curl writes a line of its own before the two of anti, whose text is
# the one of its version, so the case holds a pattern. The dots of the two
# lines stand for themselves there.
set(refusing_url "http://127.0.0.1:1/repo")
set(refusing_lines
    "anti: ${refusing_url}/com.example.units/index.toml: curl fetched nothing"
    "anti: ${refusing_url} has no version of com.example.units that satisfies every constraint on it")
list(JOIN refusing_lines "\n" refusing_pattern)
string(REPLACE "." "\\." refusing_pattern "${refusing_pattern}")
anti_message(repository_refuses
    FILES anti.toml "${manifest_package}\n[repositories]\nlocal = \"${refusing_url}\"\n\n[dependencies]\n\"com.example.units\" = { version = \"1.2.0\", repo = \"local\" }\n"
          ${app_source} "${app_text}"
    ARGS build ${tools}
    ERR_MATCH "curl: [^\n]*\n${refusing_pattern}\n")
anti_message(cache_unknown
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
    ENV --unset=HOME --unset=XDG_CACHE_HOME --unset=LOCALAPPDATA
    ARGS build ${tools}
    ERR "anti: the environment does not say where the cache of the user is"
        "${no_version}")
# R164 of "Rules of 2026-10-13" in docs/decisions.md: the manifest reader
# refuses a dependency whose name is no package name, by a repository and
# by a path alike.
anti_message(dependency_name
    FILES anti.toml "${manifest_package}\n[repositories]\nlocal = \"${repo_url}\"\n\n[dependencies]\n\"com.example.Units\" = { version = \"1.2.0\", repo = \"local\" }\n"
          ${app_source} "${app_text}"
    ARGS build ${tools}
    ERR "anti: ./anti.toml: the dependency com.example.Units is no package name")
anti_message(dependency_name_of_a_path
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com/example/units\" = { path = \"../units\" }\n"
          ${app_source} "${app_text}"
    ARGS build ${tools}
    ERR "anti: ./anti.toml: the dependency com/example/units is no package name")

# A dependency by its path: a directory of library files, or a project.
set(units_manifest "[package]\nname = \"com.example.units\"\nversion = \"0.1.0\"\n")
anti_message(path_holds_no_library
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { path = \"../libraries\" }\n"
          ${app_source} "${app_text}"
          ../libraries/notes.txt "No library file.\n"
    ARGS build ${tools}
    ERR "anti: ./../libraries holds no library file of the package com.example.units")
# A path that starts at the root is taken as it stands.
anti_message(path_from_the_root_holds_no_library
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { path = \"${WORK}/libraries\" }\n"
          ${app_source} "${app_text}"
          ../libraries/notes.txt "No library file.\n"
    ARGS build ${tools}
    ERR "anti: ${WORK}/libraries holds no library file of the package com.example.units")
anti_message(path_holds_a_damaged_library
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { path = \"../libraries\" }\n"
          ${app_source} "${app_text}"
          ../libraries/com/example/units.antl "No library file.\n"
    ARGS build ${tools}
    ERR "anti: ./../libraries/com/example/units.antl: is not a library file")
anti_message(path_holds_another_version
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { path = \"../units\", version = \"2.0.0\" }\n"
          ${app_source} "${app_text}"
          ../units/anti.toml "${units_manifest}"
          ../units/src/com/example/units.anti "${library_text}"
    ARGS build ${tools}
    ERR "anti: ./../units/dist/${HOST}/dev holds com.example.units 0.1.0, which no constraint on the package takes")

# The manifest of a path dependency is read as the one of the project.
anti_message(path_manifest_no_toml
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { path = \"../units\" }\n"
          ${app_source} "${app_text}"
          ../units/anti.toml "[package\n"
          ../units/src/com/example/units.anti "${library_text}"
    ARGS build ${tools}
    ERR "anti: ./../units/anti.toml is no TOML that anti reads")
# A module that does not parse ends the build before any module compiles.
anti_message(source_does_not_parse
    FILES anti.toml "${manifest_package}"
          ${app_source} "fn main( -> int\n{\n\treturn 0;\n}\n"
    ARGS build ${tools}
    ERR "./src/com/example/app.anti:1:10: error: expected identifier")

# The name of a source is the last part of the path of its module.
anti_message(source_name_no_identifier
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
          src/com/example/bad-name.anti "${library_text}"
    ARGS build ${tools}
    ERR "anti: `bad-name` in ./src/com/example/bad-name.anti is not a lowercase identifier")
# Two modules of a project that import each other have no order, and
# the message names the cycle, by R125 of "Rules of 2026-10-13" in
# docs/decisions.md.
anti_message(modules_import_each_other
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
          src/com/example/a.anti "//! The first.\n\nimport com.example.b;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          src/com/example/b.anti "//! The second.\n\nimport com.example.a;\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
    ARGS build ${tools}
    ERR "anti: the imports form a cycle: com.example.a -> com.example.b -> com.example.a")
# The cycle runs through a third module, which imports neither.
anti_message(modules_import_in_a_cycle_of_three
    FILES anti.toml "${manifest_package}" ${app_source} "${app_text}"
          src/com/example/a.anti "//! The first.\n\nimport com.example.c;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          src/com/example/b.anti "//! The second.\n\nimport com.example.a;\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
          src/com/example/c.anti "//! The third.\n\nimport com.example.b;\n\n/// Three.\npub fn three() -> int\n{\n\treturn 3;\n}\n"
    ARGS build ${tools}
    ERR "anti: the imports form a cycle: com.example.a -> com.example.c -> com.example.b -> com.example.a")

# `anti run --memory-checks` puts the standard error of the program through
# Anti's symbolizer. A line that is no frame, a frame without its module in
# brackets and a frame of a module that is no file or no program come out
# as they went in. ASAN_OPTIONS of the caller stays before the option of
# the run. On a Mac alone: Windows keeps the symbolizer of
# AddressSanitizer, and the Linux VM skips the option under emulation.
if(CMAKE_HOST_APPLE)
    set(frame_lines "    #0 0x1 somewhere"
                    "    #1 0x2 (/no/such/module+0x10)"
                    "    #2 0x3 (/no/such/module:arm64+0x10)"
                    "    #3 0x4 (anti.toml+0x10)"
                    "    #4 0x5 (short)")
    set(frame_text "//! A program that writes lines of the form of a frame.\n\nimport anti.io;\n\nfn main() -> int\n{\n")
    foreach(line IN LISTS frame_lines)
        string(APPEND frame_text "\tio.eprintln(\"${line}\");\n")
    endforeach()
    string(APPEND frame_text "\treturn 0;\n}\n")
    anti_message(memory_checks_lines_that_are_no_frames STATUS 0
        FILES anti.toml "${manifest_package}" ${app_source} "${frame_text}"
        ENV ASAN_OPTIONS=detect_leaks=1
        ARGS run --memory-checks ${tools}
        ERR ${frame_lines})
endif()

# The lock file. One that answers the manifest is read in place of the
# indexes.
set(lock_head "version = 1\n\n[[package]]\n")
set(lock_units "name = \"com.example.units\"\nversion = \"1.2.0\"\nrepo = \"${repo_url}\"\nmodules = [\n    ${units_module},\n]\n")
anti_message(lock_offline_no_library
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}${lock_units}"
    ARGS build --offline ${tools}
    ERR "anti: --offline and no cached com.example.units of com.example.units 1.2.0")
anti_message(lock_no_source
    FILES anti.toml "${asking_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.2.0\"\nmodules = [\n    ${units_module},\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: the lock file names com.example.units of com.example.units with no repository and no path")
anti_message(lock_version
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.x\"\nrepo = \"${repo_url}\"\nmodules = [\n    ${units_module},\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: ./anti.lock names 1.x, which is no version, so ./anti.lock is not read"
        "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
anti_message(lock_module_path
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.2.0\"\nrepo = \"${repo_url}\"\nmodules = [\n    { path = \"com/example\", sha256 = \"${units_digest}\" },\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: ./anti.lock names com/example, which is no module path, so ./anti.lock is not read"
        "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
anti_message(lock_digest
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.2.0\"\nrepo = \"${repo_url}\"\nmodules = [\n    { path = \"com.example.units\", sha256 = \"5b0c\" },\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: ./anti.lock names 5b0c, which is no SHA-256 digest, so ./anti.lock is not read"
        "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
anti_message(lock_package_name
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.2.0\"\nrepo = \"${repo_url}\"\nmodules = [\n    ${units_module},\n]\n\n[[package]]\nname = \"Com/Example\"\nversion = \"1.0.0\"\nrepo = \"${repo_url}\"\nmodules = [\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: ./anti.lock names Com/Example, which is no package name, so ./anti.lock is not read"
        "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
anti_message(lock_repository_with_a_quote
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.2.0\"\nrepo = 'file:///re\"po'\nmodules = [\n    ${units_module},\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: ./anti.lock names file:///re\"po, which is no repository URL, so ./anti.lock is not read"
        "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
# A lock file that no longer answers the manifest is passed over without a
# word, and the resolver asks the repository again.
anti_message(lock_of_another_version
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.1.0\"\nrepo = \"${repo_url}\"\nmodules = [\n    ${units_module},\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
# So is a lock file that is no TOML.
anti_message(lock_that_is_no_toml
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "][\n"
    ARGS build --offline ${tools}
    ERR "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
anti_message(lock_of_another_repository
    FILES anti.toml "${consumer_manifest}" ${app_source} "${app_text}"
          anti.lock "${lock_head}name = \"com.example.units\"\nversion = \"1.2.0\"\nrepo = \"file:///elsewhere\"\nmodules = [\n    ${units_module},\n]\n"
    ARGS build --offline ${tools}
    ERR "anti: --offline and no cached index of com.example.units from ${repo_url}"
        "${no_version}")
string(ASCII 127 delete)
anti_message(lock_path_with_a_control_byte
    FILES anti.toml "${manifest_package}\n[dependencies]\n\"com.example.units\" = { path = \"../un${delete}its\" }\n"
          ${app_source} "${app_text}"
          ../un${delete}its/anti.toml "${units_manifest}"
          ../un${delete}its/src/com/example/units.anti "${library_text}"
    ARGS build ${tools}
    ERR "anti: ./anti.lock cannot hold the path ./../un${delete}its/dist/${HOST}/dev, which holds a double quote or a control byte")

# The resolver. Each version of one package asks for the version of the
# other that asks for the other version of it, so no round ends the walk.
function(pair_index out package other first second)
    set(text "name = \"com.example.${package}\"\nrevision = 1\n")
    foreach(pair "1.0.0|${first}" "1.1.0|${second}")
        string(REPLACE "|" ";" pair "${pair}")
        list(GET pair 0 version)
        list(GET pair 1 needs)
        string(APPEND text "\n[[version]]\nversion = \"${version}\"\n"
               "modules = [{ path = \"com.example.${package}\", sha256 = \"${units_digest}\" }]\n"
               "dependencies = [{ name = \"com.example.${other}\", version = \"=${needs}\" }]\n")
    endforeach()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()
pair_index(first_index first second 1.1.0 1.0.0)
pair_index(second_index second first 1.0.0 1.1.0)
anti_message(versions_do_not_settle
    FILES anti.toml "${manifest_package}\n[repositories]\nlocal = \"${repo_url}\"\n\n[dependencies]\n\"com.example.first\" = { version = \"1.0.0\", repo = \"local\" }\n\"com.example.second\" = { version = \"1.0.0\", repo = \"local\" }\n"
          ${app_source} "${app_text}"
          ../repo/com.example.first/index.toml "${first_index}"
          ../repo/com.example.second/index.toml "${second_index}"
    ARGS build ${tools}
    ERR "anti: the dependencies of this project do not settle on one version each")

# The options of a build.
anti_message(profile_without_release STATUS 2
    ARGS build --profile-generate
    ERR "anti: --profile-generate and --profile-use build a program with --release, without --lib")
# R157 of "Rules of 2026-10-13" in docs/decisions.md: --soname and
# --bundle-runtime build a library for C, so each needs --lib.
anti_message(soname_without_lib STATUS 2
    ARGS build --soname
    ERR "anti: --soname builds a library, with --lib")
anti_message(bundle_runtime_without_lib STATUS 2
    ARGS build --bundle-runtime
    ERR "anti: --bundle-runtime builds a library, with --lib")
