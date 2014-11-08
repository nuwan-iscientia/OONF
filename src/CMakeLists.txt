# create application data for including
include (./app_config.cmake)

# generate configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app_data.h.in ${PROJECT_BINARY_DIR}/app_data.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app_data.c.in ${PROJECT_BINARY_DIR}/app_data.c)

# add main program
set(OONF_SRCS main.c 
              ${PROJECT_BINARY_DIR}/app_data.c)

# link framework libraries
include(../cmake/link_app.cmake)
