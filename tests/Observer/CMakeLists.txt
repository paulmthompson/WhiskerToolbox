if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

# Observer tests use the co-located test file from src/Observer
add_executable(test_observer
    ${CMAKE_SOURCE_DIR}/src/Observer/Observer_Data.test.cpp
)

target_link_libraries(test_observer PRIVATE 
    Catch2::Catch2WithMain 
    WhiskerToolbox::ObserverData
)

if (WIN32)
    catch_discover_tests(test_observer
        DL_PATHS
        "$ENV{PATH}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    catch_discover_tests(test_observer)
endif()
