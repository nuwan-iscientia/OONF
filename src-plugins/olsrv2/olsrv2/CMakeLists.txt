# set library parameters
SET (source olsrv2.c
            olsrv2_lan.c
            olsrv2_originator.c
            olsrv2_reader.c
            olsrv2_routing.c
            olsrv2_tc.c
            olsrv2_writer.c)

# use generic plugin maker
oonf_create_app_plugin("olsrv2" "${source}" "")
