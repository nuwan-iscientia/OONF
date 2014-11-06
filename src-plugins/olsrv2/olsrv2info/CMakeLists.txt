# set library parameters
SET (source olsrv2info.c)

# use generic plugin maker
oonf_create_app_plugin("olsrv2info" "${source}" "")
