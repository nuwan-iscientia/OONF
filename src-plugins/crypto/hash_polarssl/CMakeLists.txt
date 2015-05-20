# check for polarssl header
INCLUDE (CheckIncludeFiles)

CHECK_INCLUDE_FILES(sha1.h HAVE_SHA1_H)
CHECK_INCLUDE_FILES(sha256.h HAVE_SHA256_H)
CHECK_INCLUDE_FILES(sha512.h HAVE_SHA512_H)
IF (HAVE_SHA1_H OR HAVE_SHA256_H OR HAVE_SHA512_H)
    # set library parameters
    SET (name hash_polarssl)

    # use generic plugin maker
    oonf_create_plugin("${name}" "${name}.c" "${name}.h" "polarssl")
ENDIF()
