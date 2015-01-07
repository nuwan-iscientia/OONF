# set source files
SET (radio_only_source  radio/dlep_radio.c
                        radio/dlep_radio_interface.c
                        radio/dlep_radio_session.c) 
SET (router_only_source router/dlep_router.c
                        router/dlep_router_interface.c
                        router/dlep_router_session.c)
SET (common_source      dlep_parser.c
                        dlep_writer.c
                        dlep_static_data.c
                        dlep_bitmap.c)

# sources/includes for dlep_radio                    
SET (radio_source       ${common_source}
                        ${radio_only_source})
SET (radio_include      radio/dlep_radio.h)

# sources/includes for dlep_router
SET (router_source      ${common_source}
                        ${router_only_source})
SET (router_include router/dlep_router.h)

# sources/includes for dlep_proxy
SET (proxy_source       ${common_source}
                        ${radio_only_source}
                        ${router_only_source})
SET (proxy_include      ${radio_include}
                        ${router_include})

# use generic plugin maker for dlep-radio
oonf_create_plugin("dlep_radio" "${radio_source}" "${radio_include}" "")

# use generic plugin maker for dlep-router
oonf_create_plugin("dlep_router" "${router_source}" "${router_include}" "")

# combined radio/router plugin
oonf_create_plugin("dlep_proxy" "${proxy_source}" "${proxy_include}" "")
