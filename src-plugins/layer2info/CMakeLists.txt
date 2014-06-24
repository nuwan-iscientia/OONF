# set library name
SET (source layer2info.c)

# use generic plugin maker
oonf_create_plugin("layer2info" "${source}" "")
