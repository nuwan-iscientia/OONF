# the order of static libraries is important
# earlier libraries can use the functions of later, not the
# other way around

# link static plugins
message ("Static plugins for executables:")

# standard static linked targets
SET(OBJECT_TARGETS )
SET(EXTERNAL_LIBRARIES )

# run through list of static plugins
FOREACH(plugin ${OONF_STATIC_PLUGINS})
    IF(TARGET oonf_static_${plugin})
        message ("    Found target: oonf_static_${plugin}")  

        # Remember object targets for static plugin
        SET(OBJECT_TARGETS ${OBJECT_TARGETS} $<TARGET_OBJECTS:oonf_static_${plugin}>)
        
        # add static plugins to global dynamic target
        ADD_DEPENDENCIES(dynamic oonf_static_${plugin})
        
        # extract external libraries of plugin 
        get_property(value TARGET oonf_${plugin} PROPERTY LINK_LIBRARIES)
        FOREACH(lib ${value})
            IF(NOT "${lib}" MATCHES "^oonf_")
                message ("        Library: ${lib}")
                SET(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${lib})
            ENDIF()
        ENDFOREACH(lib)
    ELSE (TARGET oonf_static_${plugin})
        message (FATAL_ERROR "    Did not found target: oonf_static_${plugin}")
    ENDIF(TARGET oonf_static_${plugin})
ENDFOREACH(plugin)

# create executables
ADD_EXECUTABLE(${OONF_EXE}_dynamic ${OONF_SRCS}
                                   ${OBJECT_TARGETS})
ADD_EXECUTABLE(${OONF_EXE}_static  ${OONF_SRCS}
                                   ${OBJECT_TARGETS}
                                   $<TARGET_OBJECTS:oonf_static_common> 
                                   $<TARGET_OBJECTS:oonf_static_config>
                                   $<TARGET_OBJECTS:oonf_static_core>)

# Add executables to static/dynamic target
ADD_DEPENDENCIES(dynamic ${OONF_EXE}_dynamic)
ADD_DEPENDENCIES(static  ${OONF_EXE}_static)

# add path to install target
INSTALL (TARGETS ${OONF_EXE}_dynamic DESTINATION bin)
INSTALL (TARGETS ${OONF_EXE}_static  DESTINATION bin)

# link framework libraries to dynamic executable
TARGET_LINK_LIBRARIES(${OONF_EXE}_dynamic PUBLIC oonf_core
                                                 oonf_config
                                                 oonf_common)

# link external libraries directly to executable
TARGET_LINK_LIBRARIES(${OONF_EXE}_dynamic PUBLIC ${EXTERNAL_LIBRARIES})
TARGET_LINK_LIBRARIES(${OONF_EXE}_static  PUBLIC ${EXTERNAL_LIBRARIES})

# link dlopen() library
TARGET_LINK_LIBRARIES(${OONF_EXE}_dynamic PUBLIC ${CMAKE_DL_LIBS})
TARGET_LINK_LIBRARIES(${OONF_EXE}_static  PUBLIC ${CMAKE_DL_LIBS})
