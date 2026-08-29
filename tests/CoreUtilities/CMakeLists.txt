add_executable(test_CoreUtilities
    ProbeGeometry.test.cpp
)

target_link_libraries(test_CoreUtilities PRIVATE
    Catch2::Catch2WithMain
    CoreUtilities
)

target_include_directories(test_CoreUtilities PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreUtilities/include>"
)

set_target_compiler_warnings(test_CoreUtilities)

catch_discover_tests(test_CoreUtilities)
