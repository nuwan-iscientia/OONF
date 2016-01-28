# the order of static libraries is important
# earlier libraries can use the functions of later, not the
# other way around

function (oonf_create_install_target name)
    ADD_CUSTOM_TARGET(install_${name}
                      COMMAND ${CMAKE_COMMAND} 
                      -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
                      -DCOMPONENT=component_${name}
                      -P ${CMAKE_BINARY_DIR}/cmake_install.cmake)
    ADD_DEPENDENCIES(install_${name}   ${name})
    
    get_property(value TARGET ${name} PROPERTY LINK_LIBRARIES)
    FOREACH(lib ${value})
        IF (TARGET ${lib})
            ADD_DEPENDENCIES(install_${name} ${lib})
            ADD_DEPENDENCIES(install_${name} install_${lib})
        ENDIF(TARGET ${lib})
    ENDFOREACH(lib)     
endfunction (oonf_create_install_target)
    
function (oonf_create_app executable static_plugins optional_static_plugins)
    IF(VERBOSE)
        message (STATUS "Static plugins for ${executable} app:")
    ENDIF(VERBOSE)

    # standard static linked targets
    SET(OBJECT_TARGETS )
    SET(EXTERNAL_LIBRARIES )
    SET(STATIC_PLUGIN_LIST )
    
    # generate configuration file
    configure_file(${CMAKE_SOURCE_DIR}/src/app_data.c.in ${PROJECT_BINARY_DIR}/${executable}_app_data.c)

    FOREACH(plugin ${optional_static_plugins})
        IF(TARGET oonf_static_${plugin})
            list (APPEND static_plugins ${plugin})
        ELSE(TARGET oonf_static_${plugin})
            message (STATUS "    Optional plugin ${plugin} is not available for executable ${executable}")
        ENDIF(TARGET oonf_static_${plugin})
    ENDFOREACH(plugin)

    # run through list of static plugins
    FOREACH(plugin ${static_plugins})
        IF(TARGET oonf_static_${plugin})
            IF(VERBOSE)
                message (STATUS "    Found target: oonf_static_${plugin}")
            ENDIF(VERBOSE)

            # Remember object targets for static plugin
            SET(OBJECT_TARGETS ${OBJECT_TARGETS} $<TARGET_OBJECTS:oonf_static_${plugin}>)
        
            # extract external libraries of plugin
            get_property(value TARGET oonf_${plugin} PROPERTY LINK_LIBRARIES)
            FOREACH(lib ${value})
                IF(NOT "${lib}" MATCHES "^oonf_")
                    IF(VERBOSE)
                        message (STATUS "        Library: ${lib}")
                    ENDIF(VERBOSE)
                    SET(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES} ${lib})
                ENDIF()
            ENDFOREACH(lib)
        ELSE (TARGET oonf_static_${plugin})
            ADD_CUSTOM_TARGET(${executable}_dynamic ALL COMMAND false COMMENT "Plugin ${plugin} is not there, maybe a dependency is missing?")
            ADD_CUSTOM_TARGET(${executable}_static  ALL COMMAND false COMMENT "Plugin ${plugin} is not there, maybe a dependency is missing?")
            return()
        ENDIF(TARGET oonf_static_${plugin})
    ENDFOREACH(plugin)

    # create executables
    ADD_EXECUTABLE(${executable}_dynamic ${CMAKE_SOURCE_DIR}/src/main.c
                                         ${PROJECT_BINARY_DIR}/${executable}_app_data.c
                                         ${OBJECT_TARGETS})
    ADD_EXECUTABLE(${executable}_static  ${CMAKE_SOURCE_DIR}/src/main.c
                                         ${PROJECT_BINARY_DIR}/${executable}_app_data.c
                                         ${OBJECT_TARGETS}
                                         $<TARGET_OBJECTS:oonf_static_common>
                                         $<TARGET_OBJECTS:oonf_static_config>
                                         $<TARGET_OBJECTS:oonf_static_core>)

    # Add executables to static/dynamic target
    ADD_DEPENDENCIES(dynamic ${executable}_dynamic)
    ADD_DEPENDENCIES(static  ${executable}_static)
    
    # link framework libraries to dynamic executable
    TARGET_LINK_LIBRARIES(${executable}_dynamic PUBLIC oonf_core
                                                       oonf_config
                                                       oonf_common)

    # link external libraries directly to executable
    TARGET_LINK_LIBRARIES(${executable}_dynamic PUBLIC ${EXTERNAL_LIBRARIES})
    TARGET_LINK_LIBRARIES(${executable}_static  PUBLIC ${EXTERNAL_LIBRARIES})

    # link dlopen() library
    TARGET_LINK_LIBRARIES(${executable}_dynamic PUBLIC ${CMAKE_DL_LIBS})
    TARGET_LINK_LIBRARIES(${executable}_static  PUBLIC ${CMAKE_DL_LIBS})
    
    # create install targets
    INSTALL (TARGETS ${executable}_dynamic RUNTIME 
                                           DESTINATION bin 
                                           COMPONENT component_${executable}_dynamic)
    INSTALL (TARGETS ${executable}_static  RUNTIME
                                           DESTINATION bin 
                                           COMPONENT component_${executable}_static)

    # add custom install targets
    oonf_create_install_target("${executable}_dynamic")
    oonf_create_install_target("${executable}_static")
endfunction(oonf_create_app)
