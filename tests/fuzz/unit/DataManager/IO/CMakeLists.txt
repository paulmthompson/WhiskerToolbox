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

# CSV LineData single-file round-trip fuzz test

add_executable(fuzz_csv_line_single_roundtrip
    fuzz_csv_line_single_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_csv_line_single_roundtrip)

target_link_libraries(fuzz_csv_line_single_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

target_compile_options(fuzz_csv_line_single_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_line_single_roundtrip
    PRIVATE
        -fsanitize=address
)

gtest_discover_tests(fuzz_csv_line_single_roundtrip)

# CSV LineData multi-file round-trip fuzz test

add_executable(fuzz_csv_line_multi_roundtrip
    fuzz_csv_line_multi_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_csv_line_multi_roundtrip)

target_link_libraries(fuzz_csv_line_multi_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

target_compile_options(fuzz_csv_line_multi_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_line_multi_roundtrip
    PRIVATE
        -fsanitize=address
)

gtest_discover_tests(fuzz_csv_line_multi_roundtrip)

# CSV AnalogTimeSeries round-trip fuzz test

add_executable(fuzz_csv_analog_roundtrip
    fuzz_csv_analog_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_csv_analog_roundtrip)

target_link_libraries(fuzz_csv_analog_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

target_compile_options(fuzz_csv_analog_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_analog_roundtrip
    PRIVATE
        -fsanitize=address
)

gtest_discover_tests(fuzz_csv_analog_roundtrip)

# CSV DigitalEventSeries round-trip fuzz test

add_executable(fuzz_csv_event_roundtrip
    fuzz_csv_event_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_csv_event_roundtrip)

target_link_libraries(fuzz_csv_event_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
)

target_compile_options(fuzz_csv_event_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_event_roundtrip
    PRIVATE
        -fsanitize=address
)

gtest_discover_tests(fuzz_csv_event_roundtrip)
