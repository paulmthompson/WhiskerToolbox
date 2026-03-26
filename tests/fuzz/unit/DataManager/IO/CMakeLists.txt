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
        WhiskerToolbox::PointData
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
add_fuzz_test(fuzz_csv_point_roundtrip)

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
        WhiskerToolbox::LineData
)

target_compile_options(fuzz_csv_line_single_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_line_single_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_csv_line_single_roundtrip)

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
        WhiskerToolbox::LineData
)

target_compile_options(fuzz_csv_line_multi_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_line_multi_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_csv_line_multi_roundtrip)

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
        WhiskerToolbox::AnalogTimeSeries
)

target_compile_options(fuzz_csv_analog_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_analog_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_csv_analog_roundtrip)

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
        WhiskerToolbox::DigitalTimeSeries
)

target_compile_options(fuzz_csv_event_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_event_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_csv_event_roundtrip)

# CSV DigitalIntervalSeries round-trip fuzz test

add_executable(fuzz_csv_interval_roundtrip
    fuzz_csv_interval_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_csv_interval_roundtrip)

target_link_libraries(fuzz_csv_interval_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
        WhiskerToolbox::DigitalTimeSeries
)

target_compile_options(fuzz_csv_interval_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_interval_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_csv_interval_roundtrip)

# CSV MaskData RLE round-trip fuzz test

add_executable(fuzz_csv_mask_rle_roundtrip
    fuzz_csv_mask_rle_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_csv_mask_rle_roundtrip)

target_link_libraries(fuzz_csv_mask_rle_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
        WhiskerToolbox::MaskData
)

target_compile_options(fuzz_csv_mask_rle_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_csv_mask_rle_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_csv_mask_rle_roundtrip)

# CapnProto LineData round-trip fuzz test

if(ENABLE_CAPNPROTO)

add_executable(fuzz_capnproto_line_roundtrip
    fuzz_capnproto_line_roundtrip.cpp
)

add_fuzz_test_with_instantiations(fuzz_capnproto_line_roundtrip)

target_link_libraries(fuzz_capnproto_line_roundtrip
    PRIVATE
        DataManager
        DataManagerIO
        DataManagerIO_CapnProto
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
        WhiskerToolbox::LineData
)

target_compile_options(fuzz_capnproto_line_roundtrip
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_capnproto_line_roundtrip
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_capnproto_line_roundtrip)

endif()

# OpenCV MaskData image save robustness fuzz test

if(ENABLE_OPENCV)

add_executable(fuzz_opencv_mask_save
    fuzz_opencv_mask_save.cpp
)

add_fuzz_test_with_instantiations(fuzz_opencv_mask_save)

target_link_libraries(fuzz_opencv_mask_save
    PRIVATE
        DataManager
        DataManagerOpenCV
        NEURALYZER_GEOMETRY
        gtest
        fuzztest::fuzztest
        fuzztest::fuzztest_gtest_main
        WhiskerToolbox::MaskData
)

target_compile_options(fuzz_opencv_mask_save
    PRIVATE
        -fsanitize=address
)

target_link_options(fuzz_opencv_mask_save
    PRIVATE
        -fsanitize=address
)

add_fuzz_test(fuzz_opencv_mask_save)

endif()
