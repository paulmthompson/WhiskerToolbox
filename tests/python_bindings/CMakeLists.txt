if (APPLE)
    message(STATUS "Python binding tests currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Python binding tests currently not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_python_engine
    ${CMAKE_SOURCE_DIR}/src/python_bindings/PythonEngine.test.cpp
)

target_link_libraries(test_python_engine PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

catch_discover_tests(test_python_engine)

# --- Phase 2: binding round-trip tests ---
add_executable(test_python_bindings
    ${CMAKE_SOURCE_DIR}/src/python_bindings/bindings.test.cpp
)

target_link_libraries(test_python_bindings PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

catch_discover_tests(test_python_bindings)

# --- Phase 3: PythonBridge integration tests ---
add_executable(test_python_bridge
    ${CMAKE_SOURCE_DIR}/src/python_bindings/PythonBridge.test.cpp
)

target_link_libraries(test_python_bridge PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

catch_discover_tests(test_python_bridge)

# --- Phase 5: Script management & environment tests ---
add_executable(test_python_phase5
    ${CMAKE_SOURCE_DIR}/src/python_bindings/Phase5.test.cpp
)

target_link_libraries(test_python_phase5 PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

catch_discover_tests(test_python_phase5)

# --- Phase 6: Virtual environment support tests ---
add_executable(test_python_phase6
    ${CMAKE_SOURCE_DIR}/src/python_bindings/Phase6.test.cpp
)

target_link_libraries(test_python_phase6 PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

catch_discover_tests(test_python_phase6)
