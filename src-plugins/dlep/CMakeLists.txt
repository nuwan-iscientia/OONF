# set library name
SET (radio_source dlep_radio.c)

# use generic plugin maker
oonf_create_plugin("dlep_radio" "${radio_source}" "")

# set library name
SET (router_source dlep_router.c)

# use generic plugin maker
oonf_create_plugin("dlep_router" "${router_source}" "")
