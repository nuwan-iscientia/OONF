project (OONF C)
cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

###########################
#### API configuration ####
###########################

# set cached variables
include (./cmake/lib_config.cmake)

# include compiler flags
include (./cmake/cc_flags.cmake)

########################
#### Doxygen target ####
########################

find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files/framework.doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/framework.doxyfile @ONLY)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/framework.doxyfile
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif(DOXYGEN_FOUND)

#########################
#### Compile targets ####
#########################

# get repository tag and version
include (cmake/get_version.cmake)
include (${PROJECT_BINARY_DIR}/version.cmake)

# add define for length of base path
string(LENGTH "${CMAKE_SOURCE_DIR}/" BASELENGTH)
add_definitions(-DBASEPATH_LENGTH=${BASELENGTH})

# include build helper
include (cmake/OONFBuildLibrary.cmake)

# include files
include_directories(${PROJECT_BINARY_DIR})
include_directories(external)
include_directories(src-api)

if (NOT OONF_NO_TESTING)
    ENABLE_TESTING()
endif (NOT OONF_NO_TESTING)

# define static and dynamic target
add_custom_target(static)
add_custom_target(dynamic)

# start compiling core and plugins
add_subdirectory(src-api)
add_subdirectory(src-plugins)
add_subdirectory(external)
# add_subdirectory(examples)
add_subdirectory(src)

if (NOT OONF_NO_TESTING)
#    add_subdirectory(tests)
endif (NOT OONF_NO_TESTING)
