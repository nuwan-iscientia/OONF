set(TESTS test_nhdp_1
          test_nhdp_2
          test_nhdp_3
          test_olsrv2_1
          )
set (LIBS oonf_common oonf_config)

include_directories(${CMAKE_SOURCE_DIR}/src-plugins/subsystems)
foreach(TEST ${TESTS})
    oonf_create_test(${TEST} "${TEST}.c" "${LIBS}" "$<TARGET_OBJECTS:oonf_static_rfc5444_api>")
endforeach(TEST)
