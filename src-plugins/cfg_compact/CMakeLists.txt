# set library parameters
SET (source cfg_compact.c)

# use generic plugin maker
oonf_create_plugin("cfg_compact" "${source}" "")
