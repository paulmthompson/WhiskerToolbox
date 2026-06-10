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

# Suppress LeakSanitizer — CPython's internal memory arena has benign leaks
# (type registration, dict resizing) that are outside our control.
if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_python_engine PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
else()
    add_test(NAME test_python_engine_all
        COMMAND test_python_engine
    )
    set_property(TEST test_python_engine_all
        PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()


set_target_compiler_warnings(test_python_engine)

enable_whiskertoolbox_shared_test_pch(test_python_engine)

# --- Python bindings round-trip tests ---
add_executable(test_python_bindings
    ${CMAKE_SOURCE_DIR}/src/python_bindings/bindings.test.cpp
)

target_link_libraries(test_python_bindings PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_python_bindings PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
else()
    add_test(NAME test_python_bindings_all
        COMMAND test_python_bindings
    )
    set_property(TEST test_python_bindings_all
        PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()

set_target_compiler_warnings(test_python_bindings)

enable_whiskertoolbox_shared_test_pch(test_python_bindings)

# --- PythonBridge integration tests ---
add_executable(test_python_bridge
    ${CMAKE_SOURCE_DIR}/src/python_bindings/PythonBridge.test.cpp
)

target_link_libraries(test_python_bridge PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

target_link_libraries(test_python_bridge PRIVATE DataManager)

target_link_libraries(test_python_bridge PRIVATE WhiskerToolbox::AnalogTimeSeries)
target_link_libraries(test_python_bridge PRIVATE WhiskerToolbox::DigitalTimeSeries)
target_link_libraries(test_python_bridge PRIVATE WhiskerToolbox::PointData)
target_link_libraries(test_python_bridge PRIVATE WhiskerToolbox::LineData)
target_link_libraries(test_python_bridge PRIVATE WhiskerToolbox::MaskData)
target_link_libraries(test_python_bridge PRIVATE WhiskerToolbox::TensorData)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_python_bridge PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
else()
    add_test(NAME test_python_bridge_all
        COMMAND test_python_bridge
    )
    set_property(TEST test_python_bridge_all
        PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()

set_target_compiler_warnings(test_python_bridge)

enable_whiskertoolbox_shared_test_pch(test_python_bridge)

# --- Script management & environment tests ---
add_executable(test_python_phase5
    ${CMAKE_SOURCE_DIR}/src/python_bindings/Phase5.test.cpp
)

target_link_libraries(test_python_phase5 PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

target_link_libraries(test_python_phase5 PRIVATE DataManager)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_python_phase5 PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
else()
    add_test(NAME test_python_phase5_all
        COMMAND test_python_phase5
    )
    set_property(TEST test_python_phase5_all
        PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()

set_target_compiler_warnings(test_python_phase5)

enable_whiskertoolbox_shared_test_pch(test_python_phase5)

# --- Virtual environment support tests ---
add_executable(test_python_phase6
    ${CMAKE_SOURCE_DIR}/src/python_bindings/Phase6.test.cpp
)

target_link_libraries(test_python_phase6 PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::PythonBridge
)

target_link_libraries(test_python_phase6 PRIVATE DataManager)

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_python_phase6 PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
else()
    add_test(NAME test_python_phase6_all
        COMMAND test_python_phase6
    )
    set_property(TEST test_python_phase6_all
        PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()

set_target_compiler_warnings(test_python_phase6)

enable_whiskertoolbox_shared_test_pch(test_python_phase6)