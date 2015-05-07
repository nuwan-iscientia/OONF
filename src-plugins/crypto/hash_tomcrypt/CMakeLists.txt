# set library parameters
SET (name hash_tomcrypt)

# use generic plugin maker
oonf_create_plugin("${name}" "${name}.c" "${name}.h" "tomcrypt")
