# Digital Event Series JSON loader fuzz test

add_executable(fuzz_digital_event_series_json
    fuzz_digital_event_series_json.cpp
)

target_link_libraries(fuzz_digital_event_series_json
    PRIVATE
        DataManager
        gtest
        gmock
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

# Add AddressSanitizer for better bug detection
# Note: Do NOT use -fsanitize=fuzzer here - FuzzTest uses GoogleTest mode
target_compile_options(fuzz_digital_event_series_json
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_digital_event_series_json
    PRIVATE
        -fsanitize=address
)

# Register with gtest discovery
gtest_discover_tests(fuzz_digital_event_series_json)
