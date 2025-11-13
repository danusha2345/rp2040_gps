# Install script for directory: C:/msys64/home/Daniil/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Mbed TLS")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/msys64/mingw64/bin/objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/aes.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/aria.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/asn1.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/asn1write.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/base64.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/bignum.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/block_cipher.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/build_info.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/camellia.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ccm.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/chacha20.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/chachapoly.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/check_config.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/cipher.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/cmac.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/compat-2.x.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/config_adjust_x509.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/config_psa.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/constant_time.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ctr_drbg.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/debug.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/des.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/dhm.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ecdh.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ecdsa.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ecjpake.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ecp.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/entropy.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/error.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/gcm.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/hkdf.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/hmac_drbg.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/lms.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/mbedtls_config.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/md.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/md5.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/net_sockets.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/nist_kw.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/oid.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/pem.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/pk.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/pkcs12.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/pkcs5.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/pkcs7.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/platform.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/platform_time.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/platform_util.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/poly1305.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/private_access.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/psa_util.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ripemd160.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/rsa.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/sha1.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/sha256.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/sha3.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/sha512.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ssl.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ssl_cache.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ssl_cookie.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/ssl_ticket.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/threading.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/timing.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/version.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/x509.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/x509_crl.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/x509_crt.h"
    "C:/msys64/home/Daniil/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/msys64/home/Daniil/mbedtls/include/psa/build_info.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_adjust_config_dependencies.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_builtin_composites.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_builtin_primitives.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_compat.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_config.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_driver_common.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_extra.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_legacy.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_platform.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_se_driver.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_sizes.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_struct.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_types.h"
    "C:/msys64/home/Daniil/mbedtls/include/psa/crypto_values.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/msys64/home/Daniil/mbedtls/build/include/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
