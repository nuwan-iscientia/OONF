#!/bin/cmake
SET(VERSIONFILE version.cmake)

# look for git executable
SET(found_git false) 
find_program(found_git git)

IF(${found_git} STREQUAL "found_git-NOTFOUND")
  message (FATAL "Git executable not found")
ENDIF()

SET(found_tar false) 
find_program(found_tar tar)

IF(${found_tar} STREQUAL "found_tar-NOTFOUND")
  message (FATAL "Tar executable not found")
ENDIF()

# add fixed version data
FILE (COPY ${BINARY}/${VERSIONFILE} DESTINATION ${SOURCE})

# add it to git and stash it away
execute_process(COMMAND git add ${SOURCE}/${VERSIONFILE} WORKING_DIRECTORY ${SOURCE})
execute_process(COMMAND git stash create OUTPUT_VARIABLE COMMIT WORKING_DIRECTORY ${SOURCE} OUTPUT_STRIP_TRAILING_WHITESPACE)

IF (NOT TARBALL)
    SET(TARBALL "${SOURCE}/oonf_${VERSION}.${FORMAT}")
ENDIF (NOT TARBALL)

IF (NOT TARPREFIX)
    SET (TARPREFIX "oonf_${VERSION}")
ENDIF (NOT TARPREFIX)

# generate archive
execute_process(COMMAND git archive --prefix=${TARPREFIX}/ -o ${TARBALL} ${COMMIT} WORKING_DIRECTORY ${SOURCE})

# remove version file
FILE (REMOVE ${SOURCE}/${VERSIONFILE})
execute_process(COMMAND git rm --quiet ${SOURCE}/${VERSIONFILE} WORKING_DIRECTORY ${SOURCE})

message ("created ${TARBALL}")
