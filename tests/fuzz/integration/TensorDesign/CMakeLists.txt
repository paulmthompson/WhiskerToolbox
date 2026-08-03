# TensorDesign preset composition fuzz tests

add_executable(fuzz_tensor_design_registry
    fuzz_tensor_design_registry.cpp
)

add_fuzz_test_with_instantiations(fuzz_tensor_design_registry)

target_include_directories(fuzz_tensor_design_registry
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(fuzz_tensor_design_registry
    PRIVATE
        TensorDesign
        DataManager
        WhiskerToolbox::TensorData
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

if(APPLE)
    target_link_libraries(fuzz_tensor_design_registry PRIVATE -Wl,-force_load,$<TARGET_FILE:TransformsV2>)
elseif(MSVC)
    target_link_options(fuzz_tensor_design_registry PRIVATE /WHOLEARCHIVE:TransformsV2)
    target_link_libraries(fuzz_tensor_design_registry PRIVATE TransformsV2)
else()
    target_link_libraries(fuzz_tensor_design_registry PRIVATE -Wl,--whole-archive TransformsV2 -Wl,--no-whole-archive)
endif()

target_compile_options(fuzz_tensor_design_registry
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_tensor_design_registry
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_tensor_design_registry)
