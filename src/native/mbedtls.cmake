# Mbed TLS, the TLS library of anti.net and of the HTTPS client of anti,
# built for every target of the runtime tree as lib/<target>/libmbedtls.a,
# or mbedtls.lib on Windows.
#
# The source comes from the release that tools/mbedtls-pin names, which
# get-mbedtls.cmake downloads into build/deps/mbedtls and checks against
# the pinned digest.
#
# DESIGN: the release is of the 3.6 LTS line, which "Runtime archive" in
# docs/decisions.md names beside the 4.x line, whose crypto lives in the
# second library TF-PSA-Crypto. The three libraries of the release,
# mbedcrypto, mbedx509 and mbedtls, go into one archive, the one static
# library per target that docs/distribution.md lists for mbedtls. See
# docs/decisions.md.

antic_shared_path(ANTIC_MBEDTLS_DIR "${ANTIC_DEPS_DIR}/mbedtls"
    "the Mbed TLS source from src/native/get-mbedtls.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_MBEDTLS_DIR}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/get-mbedtls.cmake"
                RESULT_VARIABLE antic_mbedtls_fetched)
if(NOT antic_mbedtls_fetched EQUAL 0)
    message(FATAL_ERROR "src/native/get-mbedtls.cmake failed, so the build "
                        "has no Mbed TLS source")
endif()
file(STRINGS "${PROJECT_SOURCE_DIR}/tools/mbedtls-pin" antic_mbedtls_version
     REGEX "^MBEDTLS_VERSION=")
string(REGEX REPLACE "^MBEDTLS_VERSION=" "" antic_mbedtls_version
       "${antic_mbedtls_version}")
set(ANTIC_MBEDTLS_SOURCE "${ANTIC_MBEDTLS_DIR}/mbedtls-${antic_mbedtls_version}")
set(ANTIC_MBEDTLS_INCLUDE "${ANTIC_MBEDTLS_SOURCE}/include")
set(ANTIC_MBEDTLS_WORK "${CMAKE_BINARY_DIR}/native/mbedtls")
antic_native_license(mbedtls "${ANTIC_MBEDTLS_SOURCE}/LICENSE")

# The sources of the three libraries, src_crypto, src_x509 and src_tls of
# library/CMakeLists.txt in the release. The files of 3rdparty/ build only
# under options the default configuration leaves off.
set(ANTIC_MBEDTLS_SOURCES
    aes aesni aesce aria asn1parse asn1write base64 bignum bignum_core
    bignum_mod bignum_mod_raw block_cipher camellia ccm chacha20 chachapoly
    cipher cipher_wrap constant_time cmac ctr_drbg des dhm ecdh ecdsa ecjpake
    ecp ecp_curves entropy entropy_poll error gcm hkdf hmac_drbg lmots lms md
    md5 memory_buffer_alloc nist_kw oid padlock pem pk pk_ecc pk_wrap pkcs12
    pkcs5 pkparse pkwrite platform platform_util poly1305 psa_crypto
    psa_crypto_aead psa_crypto_cipher psa_crypto_client
    psa_crypto_driver_wrappers_no_static psa_crypto_ecp psa_crypto_ffdh
    psa_crypto_hash psa_crypto_mac psa_crypto_pake psa_crypto_rsa
    psa_crypto_random psa_crypto_se psa_crypto_slot_management
    psa_crypto_storage psa_its_file psa_util ripemd160 rsa rsa_alt_helpers
    sha1 sha256 sha512 sha3 threading timing version version_features
    pkcs7 x509 x509_create x509_crl x509_crt x509_csr x509write x509write_crt
    x509write_csr
    debug mps_reader mps_trace net_sockets ssl_cache ssl_ciphersuites
    ssl_client ssl_cookie ssl_debug_helpers_generated ssl_msg ssl_ticket
    ssl_tls ssl_tls12_client ssl_tls12_server ssl_tls13_keys
    ssl_tls13_server ssl_tls13_client ssl_tls13_generic)

# DESIGN: include/mbedtls/mbedtls_config.h of the release, unchanged, and
# no definition of our own. It holds TLS 1.2 and 1.3, the PSA crypto API,
# the sockets of net_sockets.c, the entropy of the operating system and
# no threading. It compiles with the warnings of Mbed TLS's own build,
# ANTIC_MBEDTLS_WARNINGS of src/native/warnings.cmake.

set(antic_mbedtls_probe "${PROJECT_SOURCE_DIR}/tests/abi/mbedtls_probe.c")
foreach(target IN LISTS ANTIC_NATIVE_TARGETS)
    antic_native_target(triple flags "${target}")
    antic_native_library(name "${target}" mbedtls)
    set(work "${ANTIC_MBEDTLS_WORK}/${target}")
    set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${name}")
    set(compile "${CMAKE_C_COMPILER}" --target=${triple} -std=c99 -O2
        ${flags}
        "-ffile-prefix-map=${ANTIC_MBEDTLS_SOURCE}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
        "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")
    set(objects "")
    foreach(source IN LISTS ANTIC_MBEDTLS_SOURCES)
        set(input "${ANTIC_MBEDTLS_SOURCE}/library/${source}.c")
        set(object "${work}/${source}.o")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND ${compile} ${ANTIC_MBEDTLS_WARNINGS}
                -I "${ANTIC_MBEDTLS_INCLUDE}"
                -I "${ANTIC_MBEDTLS_SOURCE}/library"
                -c "${input}" -o "${object}"
            DEPENDS "${input}"
            VERBATIM)
        list(APPEND objects "${object}")
    endforeach()
    add_custom_command(OUTPUT "${library}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${ANTIC_RUNTIME_DIR}/lib/${target}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${library}"
        COMMAND "${ANTIC_LLVM_AR}" rcs "${library}" ${objects}
        DEPENDS ${objects}
        VERBATIM)

    # The C half of the test mbedtls_link_<target>, compiled as a program
    # of that target would compile C against the library.
    set(probe "${work}/mbedtls_probe.o")
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} ${ANTIC_C_WARNINGS}
            -isystem "${ANTIC_MBEDTLS_INCLUDE}"
            -c "${antic_mbedtls_probe}" -o "${probe}"
        DEPENDS "${antic_mbedtls_probe}"
        VERBATIM)
    add_custom_target(mbedtls_${target} ALL DEPENDS "${library}" "${probe}")

    # The library links for every target, and on the host the program runs.
    set(source "${PROJECT_SOURCE_DIR}/tests/abi/mbedtls_link.anti")
    if(target STREQUAL ANTIC_HOST_TARGET)
        add_test(NAME mbedtls_run
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${source}"
                "-DOBJECTS=${probe},${library}"
                "-DWORK=${ANTIC_MBEDTLS_WORK}/run"
                -P "${PROJECT_SOURCE_DIR}/tests/run_program.cmake")
    endif()
    add_test(NAME mbedtls_link_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DOBJECTS=${probe},${library}"
            "-DWORK=${ANTIC_MBEDTLS_WORK}/link"
            "-DTARGET=${target}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_link.cmake")
endforeach()

add_test(NAME mbedtls_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_mbedtls_pin.cmake")
