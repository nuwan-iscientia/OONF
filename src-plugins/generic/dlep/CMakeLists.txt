# set source files
SET (radio_source   dlep_parser.c dlep_writer.c dlep_static_data.c dlep_bitmap.c
                    radio/dlep_radio.c
                    radio/dlep_radio_interface.c
                    radio/dlep_radio_session.c)
SET (radio_include  radio/dlep_radio.h)

SET (router_source  dlep_parser.c dlep_writer.c dlep_static_data.c dlep_bitmap.c
                    router/dlep_router.c
                    router/dlep_router_interface.c
                    router/dlep_router_session.c)
SET (router_include router/dlep_router.h)

# use generic plugin maker for dlep-radio
oonf_create_plugin("dlep_radio" "${radio_source}" "${radio_include}" "")

# use generic plugin maker for dlep-router
oonf_create_plugin("dlep_router" "${router_source}" "${router_include}" "")
