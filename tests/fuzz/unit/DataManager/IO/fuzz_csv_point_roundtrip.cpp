/**
 * @file fuzz_csv_point_roundtrip.cpp
 * @brief Fuzz tests for CSV PointData round-trip (save → load → compare)
 *
 * Verifies that saving PointData to CSV and loading it back produces
 * identical data within floating-point formatting precision.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "IO/formats/CSV/points/Point_Data_CSV.hpp"
#include "Points/Point_Data.hpp"

#include <cmath>
#include <filesystem>
#include <map>
#include <vector>

namespace {

/**
 * @brief Round-trip fuzz test: generate PointData, save as CSV, load back, compare.
 *
 * Uses the single-point-per-frame constructor to build PointData from fuzzed
 * frame indices and coordinates.  Saves with comma delimiter and loads back
 * with matching settings.
 *
 * @param frame_indices  Distinct frame numbers (clamped to non-negative).
 * @param x_coords       X coordinates, one per frame.
 * @param y_coords       Y coordinates, one per frame.
 */
void FuzzCsvPointRoundTrip(
        std::vector<int> const & frame_indices,
        std::vector<float> const & x_coords,
        std::vector<float> const & y_coords) {

    // Need at least one entry and matching sizes
    auto const n = std::min({frame_indices.size(), x_coords.size(), y_coords.size()});
    if (n == 0) {
        return;
    }

    // Skip if any coordinate is non-finite (CSV formatting can't represent NaN/Inf reliably)
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(x_coords[i]) || !std::isfinite(y_coords[i])) {
            return;
        }
    }

    // Build a map with non-negative, deduplicated frame indices
    std::map<TimeFrameIndex, Point2D<float>> input_map;
    for (size_t i = 0; i < n; ++i) {
        int const frame = std::abs(frame_indices[i]);
        input_map[TimeFrameIndex(frame)] = Point2D<float>{x_coords[i], y_coords[i]};
    }

    if (input_map.empty()) {
        return;
    }

    // Construct PointData
    PointData const point_data(input_map);

    // Save to a temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_point";
    std::filesystem::create_directories(temp_dir);

    CSVPointSaverOptions save_opts;
    save_opts.filename = "fuzz_points.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.save_header = true;
    save_opts.header = "frame,x,y";

    bool const save_ok = save(&point_data, save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back with matching settings
    CSVPointLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_points.csv").string();
    load_opts.frame_column = 0;
    load_opts.x_column = 1;
    load_opts.y_column = 2;
    load_opts.column_delim = ",";

    auto const loaded_map = load(load_opts);

    // Compare: same number of frames
    ASSERT_EQ(loaded_map.size(), input_map.size());

    // Compare each frame's coordinates within CSV formatting precision
    // Default float formatting gives ~6 significant digits
    float constexpr tolerance = 1e-3F;
    for (auto const & [frame, orig_point]: input_map) {
        auto const it = loaded_map.find(frame);
        ASSERT_NE(it, loaded_map.end()) << "Missing frame " << frame.getValue();

        auto const & loaded_point = it->second;
        EXPECT_NEAR(loaded_point.x, orig_point.x, std::max(tolerance, std::abs(orig_point.x) * 1e-5F))
                << "X mismatch at frame " << frame.getValue();
        EXPECT_NEAR(loaded_point.y, orig_point.y, std::max(tolerance, std::abs(orig_point.y) * 1e-5F))
                << "Y mismatch at frame " << frame.getValue();
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvPointRoundTripFuzz, FuzzCsvPointRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100000)).WithMinSize(1).WithMaxSize(200),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(200),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(200));

/**
 * @brief Fuzz test with varying delimiter settings.
 *
 * Tests that saving with a non-default delimiter and loading with the same
 * delimiter also produces correct round-trips.
 */
void FuzzCsvPointRoundTripDelimiter(
        std::vector<int> const & frame_indices,
        std::vector<float> const & x_coords,
        std::vector<float> const & y_coords,
        std::string const & delimiter) {

    auto const n = std::min({frame_indices.size(), x_coords.size(), y_coords.size()});
    if (n == 0 || delimiter.empty()) {
        return;
    }

    // Skip problematic delimiters (newlines, digits, dots, minus signs break parsing)
    for (char const c: delimiter) {
        if (c == '\n' || c == '\r' || std::isdigit(static_cast<unsigned char>(c)) ||
            c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
            return;
        }
    }

    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(x_coords[i]) || !std::isfinite(y_coords[i])) {
            return;
        }
    }

    std::map<TimeFrameIndex, Point2D<float>> input_map;
    for (size_t i = 0; i < n; ++i) {
        int const frame = std::abs(frame_indices[i]);
        input_map[TimeFrameIndex(frame)] = Point2D<float>{x_coords[i], y_coords[i]};
    }

    if (input_map.empty()) {
        return;
    }

    PointData const point_data(input_map);

    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_point_delim";
    std::filesystem::create_directories(temp_dir);

    CSVPointSaverOptions save_opts;
    save_opts.filename = "fuzz_points_delim.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = delimiter;
    save_opts.save_header = false;// Skip header to avoid delimiter in header mismatch

    bool const save_ok = save(&point_data, save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    CSVPointLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_points_delim.csv").string();
    load_opts.frame_column = 0;
    load_opts.x_column = 1;
    load_opts.y_column = 2;
    // The loader takes a single char delimiter
    load_opts.column_delim = delimiter.substr(0, 1);

    auto const loaded_map = load(load_opts);

    // With single-char delimiter only, we should get the same data back
    if (delimiter.size() == 1) {
        ASSERT_EQ(loaded_map.size(), input_map.size());

        float constexpr tolerance = 1e-3F;
        for (auto const & [frame, orig_point]: input_map) {
            auto const it = loaded_map.find(frame);
            ASSERT_NE(it, loaded_map.end());
            EXPECT_NEAR(it->second.x, orig_point.x, std::max(tolerance, std::abs(orig_point.x) * 1e-5F));
            EXPECT_NEAR(it->second.y, orig_point.y, std::max(tolerance, std::abs(orig_point.y) * 1e-5F));
        }
    }

    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvPointRoundTripFuzz, FuzzCsvPointRoundTripDelimiter)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 10000)).WithMinSize(1).WithMaxSize(50),
                fuzztest::VectorOf(fuzztest::InRange(-1e3F, 1e3F)).WithMinSize(1).WithMaxSize(50),
                fuzztest::VectorOf(fuzztest::InRange(-1e3F, 1e3F)).WithMinSize(1).WithMaxSize(50),
                fuzztest::OneOf(
                        fuzztest::Just(std::string(",")),
                        fuzztest::Just(std::string(" ")),
                        fuzztest::Just(std::string("\t")),
                        fuzztest::Just(std::string(";"))));

}// namespace
