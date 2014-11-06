# set library parameters
SET (source nhdpinfo.c)

# use generic plugin maker
oonf_create_app_plugin("nhdpinfo" "${source}" "")
