#!/bin/cmake
IF(EXISTS "${CMAKE_SOURCE_DIR}/version.cmake")
  # preconfigured version data
  FILE (COPY ${CMAKE_SOURCE_DIR}/version.cmake DESTINATION ${PROJECT_BINARY_DIR})
ELSEIF(NOT OONF_LIB_GIT OR NOT OONF_VERSION)
  # look for git executable
  SET(found_git false) 
  find_program(found_git git)

  SET(OONF_LIB_GIT "cannot read git repository")

  IF(NOT ${found_git} STREQUAL "found_git-NOTFOUND")
    # get git description WITH dirty flag
    execute_process(COMMAND git describe --always --long --tags --dirty --match "v[0-9]*"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE LIB_GIT OUTPUT_STRIP_TRAILING_WHITESPACE)

    # get git description WITH dirty flag
    execute_process(COMMAND git describe --abbrev=0 --match "v[0-9]*"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE VERSION_TAG OUTPUT_STRIP_TRAILING_WHITESPACE)
    
    # strip "v" from tag
    string(SUBSTRING ${VERSION_TAG} 1 -1 VERSION)
  ENDIF()
  
  message ("Git commit: ${LIB_GIT}, Git version: ${VERSION}")
  configure_file (${CMAKE_SOURCE_DIR}/cmake/version.cmake.in ${PROJECT_BINARY_DIR}/version.cmake)
ENDIF()
