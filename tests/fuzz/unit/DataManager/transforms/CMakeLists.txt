
add_executable(fuzz_v2_parameter_io
    fuzz_v2_parameter_io.cpp
)

add_fuzz_test_with_instantiations(fuzz_v2_parameter_io)

target_link_libraries(fuzz_v2_parameter_io
    PRIVATE
        DataManager
        gtest
        gmock
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

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
