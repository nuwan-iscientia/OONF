# add test directories
include_directories(.)

function (oonf_create_test executable source libraries objects)
    # create executable
    ADD_EXECUTABLE(${executable} ${source} ${objects})

    add_dependencies(build_tests ${executable})

    TARGET_LINK_LIBRARIES(${executable} ${libraries})
    TARGET_LINK_LIBRARIES(${executable} static_cunit)

    # link regex for windows and android
    IF (WIN32 OR ANDROID)
        TARGET_LINK_LIBRARIES(${executable} oonf_regex)
    ENDIF(WIN32 OR ANDROID)

    # link extra win32 libs
    IF(WIN32)
        SET_TARGET_PROPERTIES(${executable} PROPERTIES ENABLE_EXPORTS true)
        TARGET_LINK_LIBRARIES(${executable} ws2_32 iphlpapi)
    ENDIF(WIN32)

    ADD_TEST(NAME ${executable} COMMAND ${executable})
endfunction (oonf_create_test)

add_subdirectory(cunit)
add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(rfc5444)
