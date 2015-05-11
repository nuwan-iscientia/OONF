# check for tomcrypt header
INCLUDE (CheckIncludeFiles)

CHECK_INCLUDE_FILES(tomcrypt.h HAVE_TOMCRYPT_H)
IF (HAVE_TOMCRYPT_H)
    # set library parameters
    SET (name hash_tomcrypt)

    # use generic plugin maker
    oonf_create_plugin("${name}" "${name}.c" "${name}.h" "tomcrypt")
ENDIF(HAVE_TOMCRYPT_H)
