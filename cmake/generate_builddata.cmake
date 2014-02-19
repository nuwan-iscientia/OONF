#!/bin/cmake

# look for git executable
IF(NOT OONF_LIB_GIT)
  SET(found_git false) 
  find_program(found_git git)

  SET(OONF_LIB_GIT "cannot read git repository")

  IF(NOT ${found_git} STREQUAL "found_git-NOTFOUND")
	  # get git description WITH dirty flag
	  execute_process(COMMAND git describe --always --long --tags --dirty --match "v[0-9]*"
		  OUTPUT_VARIABLE OONF_LIB_GIT OUTPUT_STRIP_TRAILING_WHITESPACE)
  ENDIF()
  
ENDIF()

message ("Git commit: ${OONF_LIB_GIT}")

# create builddata file
configure_file (${SRC} ${DST})
