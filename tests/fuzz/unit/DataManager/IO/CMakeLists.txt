# CSV PointData round-trip fuzz test

add_executable(fuzz_csv_point_roundtrip
    fuzz_csv_point_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_csv_point_roundtrip)

target_link_libraries(fuzz_csv_point_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

# Add AddressSanitizer for better bug detection
# Note: Do NOT use -fsanitize=fuzzer here - FuzzTest uses GoogleTest mode
target_compile_options(fuzz_csv_point_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_point_roundtrip
    PRIVATE
        -fsanitize=address
)

# Register with gtest discovery
gtest_discover_tests(fuzz_csv_point_roundtrip)
