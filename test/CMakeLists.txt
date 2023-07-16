include_directories(${CMAKE_SOURCE_DIR}/googletest/googletest/include)

include(GoogleTest)

file(GLOB_RECURSE TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*test.cpp")

# #########################################
# "make XYZ_test"
# #########################################
foreach (test_file ${TEST_FILES})
    # replace "XYZ-test.cpp" with "XYZ-test"
    get_filename_component(test_filename ${test_file} NAME)
    string(REPLACE ".cpp" "" test_name ${test_filename})

    # Add the test target separately and as part of "make XYZ-test".
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} gtest gmock_main library)

    message(STATUS "ADD TEST: " ${test_name})

    gtest_discover_tests(${test_name}
            EXTRA_ARGS
            --gtest_color=auto
            --gtest_output=xml:${CMAKE_BINARY_DIR}/test/${test_name}.xml
            --gtest_catch_exceptions=0
            DISCOVERY_TIMEOUT 120
            PROPERTIES
            TIMEOUT 120
            )

    set_target_properties(${test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
            COMMAND ${test_name}
            )
endforeach ()