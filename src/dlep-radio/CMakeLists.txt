# create application data for including
include (./dlep_config.cmake)

# link framework libraries
include(../../cmake/link_app.cmake)

oonf_create_app("${OONF_EXE}" "${OONF_STATIC_PLUGINS}")
 