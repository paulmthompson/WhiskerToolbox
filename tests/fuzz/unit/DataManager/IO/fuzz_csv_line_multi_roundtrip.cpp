/**
 * @file fuzz_csv_line_multi_roundtrip.cpp
 * @brief Fuzz tests for CSV LineData multi-file round-trip (save -> load -> compare)
 *
 * Verifies that saving LineData to per-frame CSV files and loading them back
 * produces identical data within floating-point formatting precision.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "IO/formats/CSV/lines/Line_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"

#include <cmath>
#include <filesystem>
#include <map>
#include <vector>

namespace {

/**
 * @brief Round-trip fuzz test: generate LineData, save as multi-file CSV, load back, compare.
 *
 * Builds one line per frame from fuzzed coordinates. Saves each frame to its own
 * CSV file and loads the directory back.
 *
 * Note: The multi-file saver only saves one line per frame (the first entity at each
 * timestamp via flattened_data). The fuzz test generates one line per unique frame.
 *
 * @param frame_indices  Frame numbers (clamped to non-negative, deduplicated).
 * @param x_coords       X coordinates.
 * @param y_coords       Y coordinates.
 * @param points_per_line Number of points per line.
 */
void FuzzCsvLineMultiRoundTrip(
        std::vector<int> const & frame_indices,
        std::vector<float> const & x_coords,
        std::vector<float> const & y_coords,
        int points_per_line) {

    if (frame_indices.empty() || x_coords.empty() || y_coords.empty()) {
        return;
    }

    points_per_line = std::clamp(points_per_line, 1, 20);

    // Skip if any coordinate is non-finite
    for (auto const & x: x_coords) {
        if (!std::isfinite(x)) return;
    }
    for (auto const & y: y_coords) {
        if (!std::isfinite(y)) return;
    }

    auto const coord_count = std::min(x_coords.size(), y_coords.size());
    auto const num_lines = std::min(frame_indices.size(), coord_count / static_cast<size_t>(points_per_line));
    if (num_lines == 0) {
        return;
    }

    // Build LineData with one line per unique frame
    std::map<TimeFrameIndex, std::vector<Line2D>> input_map;
    for (size_t i = 0; i < num_lines; ++i) {
        int const frame = std::abs(frame_indices[i]);
        auto const key = TimeFrameIndex(frame);

        // Multi-file saver writes one file per (time, entity) from flattened_data,
        // but the loader puts all points from one file into one line.
        // To get an exact round-trip, only store one line per frame.
        if (input_map.count(key) > 0) {
            continue;
        }

        std::vector<Point2D<float>> points;
        points.reserve(static_cast<size_t>(points_per_line));
        for (int p = 0; p < points_per_line; ++p) {
            size_t const idx = i * static_cast<size_t>(points_per_line) + static_cast<size_t>(p);
            points.emplace_back(x_coords[idx], y_coords[idx]);
        }
        input_map[key].emplace_back(std::move(points));
    }

    if (input_map.empty()) {
        return;
    }

    LineData const line_data(input_map);

    // Save to temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_line_multi";
    // Clean up any previous run
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

    CSVMultiFileLineSaverOptions save_opts;
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.save_header = true;
    save_opts.header = "X,Y";
    save_opts.precision = 4;
    save_opts.frame_id_padding = 7;
    save_opts.overwrite_existing = true;

    bool const save_ok = save(&line_data, save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back
    CSVMultiFileLineLoaderOptions load_opts;
    load_opts.parent_dir = temp_dir.string();
    load_opts.delimiter = ",";
    load_opts.x_column = 0;
    load_opts.y_column = 1;
    load_opts.has_header = true;

    auto const loaded_map = load(load_opts);

    // Compare: same number of frames
    ASSERT_EQ(loaded_map.size(), input_map.size());

    float constexpr tolerance = 1e-3F;
    for (auto const & [frame, orig_lines]: input_map) {
        auto const it = loaded_map.find(frame);
        ASSERT_NE(it, loaded_map.end()) << "Missing frame " << frame.getValue();

        auto const & loaded_lines = it->second;
        // Multi-file saver writes one line per timestamp
        ASSERT_EQ(loaded_lines.size(), 1u)
                << "Expected exactly 1 line at frame " << frame.getValue();
        ASSERT_EQ(orig_lines.size(), 1u);

        auto const & orig_line = orig_lines[0];
        auto const & loaded_line = loaded_lines[0];

        ASSERT_EQ(loaded_line.size(), orig_line.size())
                << "Point count mismatch at frame " << frame.getValue();

        for (size_t p = 0; p < orig_line.size(); ++p) {
            EXPECT_NEAR(loaded_line[p].x, orig_line[p].x,
                        std::max(tolerance, std::abs(orig_line[p].x) * 1e-4F))
                    << "X mismatch at frame " << frame.getValue() << " point " << p;
            EXPECT_NEAR(loaded_line[p].y, orig_line[p].y,
                        std::max(tolerance, std::abs(orig_line[p].y) * 1e-4F))
                    << "Y mismatch at frame " << frame.getValue() << " point " << p;
        }
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvLineMultiRoundTripFuzz, FuzzCsvLineMultiRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 10000)).WithMinSize(1).WithMaxSize(50),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(1000),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(1000),
                fuzztest::InRange(1, 20));

}// namespace
