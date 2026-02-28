
add_executable(fuzz_v2_parameter_io
    fuzz_v2_parameter_io.cpp
    fuzz_v2_pipeline_loader.cpp
)

add_fuzz_test_with_instantiations(fuzz_v2_parameter_io)

# Force whole-archive linking for TransformsV2 to ensure static transform registration
# (RegisteredTransforms.cpp has file-scope RAII objects that register transforms into
# ElementRegistry, but the linker would strip them since fuzz tests have no direct symbol references)
if(APPLE)
    set(TRANSFORMS_V2_LINK -Wl,-force_load,$<TARGET_FILE:TransformsV2>)
elseif(MSVC)
    set(TRANSFORMS_V2_LINK TransformsV2)
    set(TRANSFORMS_V2_LINK_OPTIONS /WHOLEARCHIVE:TransformsV2)
else()
    # Linux/GCC/Clang
    set(TRANSFORMS_V2_LINK -Wl,--whole-archive TransformsV2 -Wl,--no-whole-archive)
endif()

target_link_libraries(fuzz_v2_parameter_io
    PRIVATE
        DataManager
        WhiskerToolbox::TensorData
        ${TRANSFORMS_V2_LINK}
        gtest
        gmock
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

if(MSVC)
    target_link_options(fuzz_v2_parameter_io PRIVATE ${TRANSFORMS_V2_LINK_OPTIONS})
endif()

# Add AddressSanitizer for better bug detection
# Note: Do NOT use -fsanitize=fuzzer here - FuzzTest uses GoogleTest mode
target_compile_options(fuzz_v2_parameter_io
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_v2_parameter_io
    PRIVATE
        -fsanitize=address
)

# Register with gtest discovery
gtest_discover_tests(fuzz_v2_parameter_io)
