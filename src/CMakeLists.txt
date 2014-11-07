# create application data for including
include (./app_config.cmake)

# generate configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app_data.h.in ${PROJECT_BINARY_DIR}/app_data.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app_data.c.in ${PROJECT_BINARY_DIR}/app_data.c)

# add main program
set(OONF_SRCS main.c 
              ${PROJECT_BINARY_DIR}/app_data.c)

# create executable
ADD_EXECUTABLE(${OONF_EXE} ${OONF_SRCS})

# add path to install target
INSTALL (TARGETS ${OONF_EXE} DESTINATION bin)

# link framework libraries (choose one)
IF (OONF_FRAMEWORD_DYNAMIC)
    include(../cmake/link_app_dynamic.cmake)
ELSE (OONF_FRAMEWORD_DYNAMIC)
    include(../cmake/link_app_static.cmake)
endif(OONF_FRAMEWORD_DYNAMIC)
