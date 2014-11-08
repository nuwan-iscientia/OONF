# generic oonf library creation

function (oonf_internal_create_plugin libname source include link_internal linkto_external)
    # create static and dynamic library
    add_library(oonf_${libname} SHARED ${source})
    add_library(oonf_static_${libname} OBJECT ${source})

    # add libraries to global static/dynamic target
    add_dependencies(dynamic oonf_${libname})
    add_dependencies(static oonf_static_${libname})
    
    # and link their dependencies
    if(WIN32)
        target_link_libraries(oonf_${libname} ws2_32 iphlpapi)
    endif(WIN32)

    set_target_properties(oonf_${libname} PROPERTIES SOVERSION "${OONF_VERSION}")

    if (linkto_internal)
        target_link_libraries(oonf_${libname} ${linkto_internal})
    endif (linkto_internal)
    if (linkto_external)
        target_link_libraries(oonf_${libname} ${linkto_external})
    endif (linkto_external)
    
    foreach(inc ${include})
        get_filename_component(path "${inc}" PATH)
        
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${inc}")
            install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/${inc}
                     DESTINATION ${INSTALL_INCLUDE_DIR}/${libname}/${path})
        ELSE (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${inc}")
            install (FILES ${CMAKE_BINARY_DIR}/${libname}/${inc}
                     DESTINATION ${INSTALL_INCLUDE_DIR}/${libname}/${path})
        ENDIF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${inc}")
    endforeach(inc)
endfunction (oonf_internal_create_plugin)

function (oonf_create_library libname source include linkto_internal linkto_external)
    oonf_internal_create_plugin("${libname}" "${source}" "${include}" "${linkto_internal}" "${linkto_external}")
endfunction (oonf_create_library)

function (oonf_create_plugin libname source include linkto_external)
    SET (linkto_internal oonf_core oonf_config oonf_common)
    
    oonf_create_library("${libname}" "${source}" "" "${linkto_internal}" "${linkto_external}")
endfunction (oonf_create_plugin)
