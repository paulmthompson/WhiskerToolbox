if (APPLE)
    message(STATUS "Testing Currenly not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currenly not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_gather_result
        GatherResult.test.cpp
        GatherResult_ValueStore.test.cpp
)

target_link_libraries(test_gather_result PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test_gather_result PRIVATE GatherResult TransformsV2 DataManager)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_gather_result
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_gather_result)
    endif()
else()
    add_test(NAME test_gather_result_all
        COMMAND test_gather_result
        )
endif()

