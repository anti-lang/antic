# Check the CodeView record of a Windows executable against its PDB.
#
#   cmake -DBINARY=<file.exe> -DPDB=<file.pdb> -DREADOBJ=<llvm-readobj>
#         -P tools/check-pdb.cmake
#
# A Windows program carries no symbol table. What ties it to the symbols
# of its own build is the CodeView record of its debug directory, which
# holds the GUID of the PDB and the name of the file. The link passes
# /PDBALTPATH:%_PDB%, so that name is a file name and never a path of the
# machine that linked it. This reads both ends and refuses a pair that
# does not match: the record names the PDB beside it, and the GUID of the
# record is the GUID the PDB carries in its info stream.
cmake_minimum_required(VERSION 3.20)

foreach(name BINARY PDB READOBJ)
    if(NOT DEFINED ${name})
        message(FATAL_ERROR "usage: cmake -DBINARY=<file.exe> -DPDB=<file.pdb> "
                            "-DREADOBJ=<llvm-readobj> -P tools/check-pdb.cmake")
    endif()
endforeach()
if(NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "${BINARY} does not exist")
endif()
if(NOT EXISTS "${PDB}")
    message(FATAL_ERROR "${BINARY} has no PDB at ${PDB}")
endif()

# The little-endian 32-bit word at a byte offset of a hex string, which is
# what file(READ ... HEX) writes.
function(word hex offset out)
    math(EXPR at "${offset} * 2")
    string(SUBSTRING "${hex}" ${at} 2 b0)
    math(EXPR at "${at} + 2")
    string(SUBSTRING "${hex}" ${at} 2 b1)
    math(EXPR at "${at} + 2")
    string(SUBSTRING "${hex}" ${at} 2 b2)
    math(EXPR at "${at} + 2")
    string(SUBSTRING "${hex}" ${at} 2 b3)
    math(EXPR value "0x${b3}${b2}${b1}${b0}")
    set("${out}" "${value}" PARENT_SCOPE)
endfunction()

# The GUID of the info stream of an MSF file, in the form llvm-readobj
# prints: the first three fields of a GUID are a word and two halves, and
# they read little-endian, while the last eight bytes read in order.
#
# An MSF file is a sequence of blocks. The superblock names the size of a
# block and the block that holds the map of the stream directory; the
# directory names the size and the blocks of every stream; and stream one
# is the info stream, whose GUID stands twelve bytes in, after the
# version, the signature and the age. The stream begins at a block
# boundary and a block is 512 bytes at the least, so those sixteen bytes
# lie in one block and one read reaches them.
function(pdb_guid path out)
    file(READ "${path}" super HEX OFFSET 0 LIMIT 56)
    string(SUBSTRING "${super}" 0 64 magic)
    if(NOT magic STREQUAL "4d6963726f736f667420432f432b2b204d534620372e30300d0a1a4453000000")
        message(FATAL_ERROR "${path} is no MSF file")
    endif()
    word("${super}" 32 block_size)
    word("${super}" 44 directory_bytes)
    word("${super}" 52 map_block)

    # The map is a list of the blocks the directory lies in.
    math(EXPR map_at "${map_block} * ${block_size}")
    math(EXPR map_words "(${directory_bytes} + ${block_size} - 1) / ${block_size}")
    math(EXPR map_bytes "${map_words} * 4")
    file(READ "${path}" map HEX OFFSET ${map_at} LIMIT ${map_bytes})

    # The directory itself, which those blocks hold end to end.
    set(directory "")
    math(EXPR last "${map_words} - 1")
    foreach(i RANGE ${last})
        math(EXPR at "${i} * 4")
        word("${map}" ${at} block)
        math(EXPR block_at "${block} * ${block_size}")
        file(READ "${path}" part HEX OFFSET ${block_at} LIMIT ${block_size})
        string(APPEND directory "${part}")
    endforeach()

    # NumStreams, then the size of each stream, then the blocks of each.
    # Stream zero's blocks come first, so stream one's first block stands
    # after them.
    word("${directory}" 0 streams)
    if(streams LESS 2)
        message(FATAL_ERROR "${path} holds ${streams} streams and no info stream")
    endif()
    word("${directory}" 4 size_zero)
    math(EXPR blocks_zero "(${size_zero} + ${block_size} - 1) / ${block_size}")
    math(EXPR at "4 + ${streams} * 4 + ${blocks_zero} * 4")
    word("${directory}" ${at} info_block)

    math(EXPR guid_at "${info_block} * ${block_size} + 12")
    file(READ "${path}" guid HEX OFFSET ${guid_at} LIMIT 16)
    string(TOUPPER "${guid}" guid)
    set(a "")
    foreach(at 6 4 2 0)
        string(SUBSTRING "${guid}" ${at} 2 byte)
        string(APPEND a "${byte}")
    endforeach()
    set(b "")
    foreach(at 10 8)
        string(SUBSTRING "${guid}" ${at} 2 byte)
        string(APPEND b "${byte}")
    endforeach()
    set(c "")
    foreach(at 14 12)
        string(SUBSTRING "${guid}" ${at} 2 byte)
        string(APPEND c "${byte}")
    endforeach()
    string(SUBSTRING "${guid}" 16 4 d)
    string(SUBSTRING "${guid}" 20 12 e)
    set("${out}" "{${a}-${b}-${c}-${d}-${e}}" PARENT_SCOPE)
endfunction()

execute_process(COMMAND "${READOBJ}" --coff-debug-directory "${BINARY}"
                RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "llvm-readobj read no debug directory of ${BINARY}\n${out}${err}")
endif()
if(NOT out MATCHES "PDBGUID: (\\{[0-9A-Fa-f-]+\\})")
    message(FATAL_ERROR "${BINARY} carries no CodeView record\n${out}")
endif()
set(record "${CMAKE_MATCH_1}")
if(NOT out MATCHES "PDBFileName: ([^\n]+)\n")
    message(FATAL_ERROR "the CodeView record of ${BINARY} names no PDB\n${out}")
endif()
set(named "${CMAKE_MATCH_1}")

# /PDBALTPATH:%_PDB% leaves the file name alone, so a separator here is
# the path of the machine that linked the program.
if(named MATCHES "[/\\\\]")
    message(FATAL_ERROR "${BINARY} names the PDB as `${named}`, which is a "
                        "path of the machine that linked it")
endif()
get_filename_component(want "${PDB}" NAME)
if(NOT named STREQUAL "${want}")
    message(FATAL_ERROR "${BINARY} names `${named}` and its PDB is `${want}`")
endif()

pdb_guid("${PDB}" carried)
if(NOT record STREQUAL "${carried}")
    message(FATAL_ERROR "${BINARY} carries the CodeView GUID ${record} and "
                        "${want} carries ${carried}")
endif()
message(STATUS "${want} ${carried}")
