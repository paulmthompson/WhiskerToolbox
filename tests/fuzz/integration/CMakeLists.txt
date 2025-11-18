# Integration fuzz tests for complex workflows

# Transform Pipeline fuzz test
add_executable(fuzz_transform_pipeline
    fuzz_transform_pipeline.cpp
)

target_link_libraries(fuzz_transform_pipeline
    PRIVATE
        DataManager
        gtest
        gtest_main
        gmock
        fuzztest::fuzztest
)

# Add AddressSanitizer for better bug detection
# Note: Do NOT use -fsanitize=fuzzer here - FuzzTest uses GoogleTest mode
target_compile_options(fuzz_transform_pipeline
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_transform_pipeline
    PRIVATE
        -fsanitize=address
)

# Register with gtest discovery
gtest_discover_tests(fuzz_transform_pipeline)
