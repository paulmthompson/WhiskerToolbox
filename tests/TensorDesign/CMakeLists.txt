if (APPLE)
    message(STATUS "Testing Currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "Testing Currently not supported on Windows")
    return()
endif()

add_executable(test_tensor_design
    test_column_recipe_preset_registry.cpp
    test_tensor_design_builder.cpp
    test_tensor_whisker_contact_scenario.cpp
)

target_include_directories(test_tensor_design PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/DataManager>"
)

target_link_libraries(test_tensor_design PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test_tensor_design PRIVATE TensorDesign)
target_link_libraries(test_tensor_design PRIVATE DataManager)
target_link_libraries(test_tensor_design PRIVATE WhiskerToolbox::TensorData)

if(APPLE)
    target_link_libraries(test_tensor_design PRIVATE -Wl,-force_load,$<TARGET_FILE:TransformsV2>)
elseif(MSVC)
    target_link_options(test_tensor_design PRIVATE /WHOLEARCHIVE:TransformsV2)
    target_link_libraries(test_tensor_design PRIVATE TransformsV2)
else()
    target_link_libraries(test_tensor_design PRIVATE -Wl,--whole-archive TransformsV2 -Wl,--no-whole-archive)
endif()

if(ENABLE_DETAILED_TEST_DISCOVERY)
    catch_discover_tests(test_tensor_design)
else()
    add_test(NAME test_tensor_design_all COMMAND test_tensor_design)
endif()

set_target_compiler_warnings(test_tensor_design)
enable_whiskertoolbox_shared_test_pch(test_tensor_design)
