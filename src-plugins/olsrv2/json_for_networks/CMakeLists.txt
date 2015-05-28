# set library parameters
SET (name json_for_networks)

# use generic plugin maker
oonf_create_plugin("${name}" "${name}.c" "${name}.h" "")
