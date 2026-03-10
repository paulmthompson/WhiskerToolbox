/**
 * @file fuzz_csv_line_single_roundtrip.cpp
 * @brief Fuzz tests for CSV LineData single-file round-trip (save -> load -> compare)
 *
 * Verifies that saving LineData to a single CSV file and loading it back
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
 * @brief Round-trip fuzz test: generate LineData, save as single CSV, load back, compare.
 *
 * Builds LineData from fuzzed frame indices and coordinate vectors.
 * Saves with comma delimiter and loads back with matching settings.
 *
 * @param frame_indices  Frame numbers for each line (clamped to non-negative).
 * @param x_coords       Flat X coordinates for all lines (split into groups by points_per_line).
 * @param y_coords       Flat Y coordinates for all lines.
 * @param points_per_line Number of points per line (1-20).
 */
void FuzzCsvLineSingleRoundTrip(
        std::vector<int> const & frame_indices,
        std::vector<float> const & x_coords,
        std::vector<float> const & y_coords,
        int points_per_line) {

    if (frame_indices.empty() || x_coords.empty() || y_coords.empty()) {
        return;
    }

    // Clamp points per line to a reasonable range
    points_per_line = std::clamp(points_per_line, 1, 20);

    // Skip if any coordinate is non-finite
    for (auto const & x: x_coords) {
        if (!std::isfinite(x)) return;
    }
    for (auto const & y: y_coords) {
        if (!std::isfinite(y)) return;
    }

    // Build LineData: each frame gets one line with points_per_line points
    auto const coord_count = std::min(x_coords.size(), y_coords.size());
    auto const num_lines = std::min(frame_indices.size(), coord_count / static_cast<size_t>(points_per_line));
    if (num_lines == 0) {
        return;
    }

    // Use map to deduplicate frame indices (last line per frame wins — matching CSV save behavior
    // which iterates flattened_data and overwrites per-frame in the file)
    std::map<TimeFrameIndex, std::vector<Line2D>> input_map;
    for (size_t i = 0; i < num_lines; ++i) {
        int const frame = std::abs(frame_indices[i]);
        std::vector<Point2D<float>> points;
        points.reserve(static_cast<size_t>(points_per_line));
        for (int p = 0; p < points_per_line; ++p) {
            size_t const idx = i * static_cast<size_t>(points_per_line) + static_cast<size_t>(p);
            points.emplace_back(x_coords[idx], y_coords[idx]);
        }
        // Append line at this frame
        input_map[TimeFrameIndex(frame)].emplace_back(std::move(points));
    }

    if (input_map.empty()) {
        return;
    }

    // Construct LineData
    LineData const line_data(input_map);

    // Save to temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_line_single";
    std::filesystem::create_directories(temp_dir);

    CSVSingleFileLineSaverOptions save_opts;
    save_opts.filename = "fuzz_lines.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.save_header = true;
    save_opts.header = "Frame,X,Y";
    save_opts.precision = 4;

    bool const save_ok = save(&line_data, save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back with matching settings
    CSVSingleFileLineLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_lines.csv").string();
    load_opts.delimiter = ",";
    load_opts.coordinate_delimiter = ",";
    load_opts.has_header = true;
    load_opts.header_identifier = "Frame";

    auto const loaded_map = load(load_opts);

    // Compare: same number of frames
    ASSERT_EQ(loaded_map.size(), input_map.size());

    // Compare each frame's lines within CSV formatting precision
    float constexpr tolerance = 1e-3F;
    for (auto const & [frame, orig_lines]: input_map) {
        auto const it = loaded_map.find(frame);
        ASSERT_NE(it, loaded_map.end()) << "Missing frame " << frame.getValue();

        auto const & loaded_lines = it->second;
        ASSERT_EQ(loaded_lines.size(), orig_lines.size())
                << "Line count mismatch at frame " << frame.getValue();

        for (size_t line_idx = 0; line_idx < orig_lines.size(); ++line_idx) {
            auto const & orig_line = orig_lines[line_idx];
            auto const & loaded_line = loaded_lines[line_idx];

            ASSERT_EQ(loaded_line.size(), orig_line.size())
                    << "Point count mismatch at frame " << frame.getValue()
                    << " line " << line_idx;

            for (size_t p = 0; p < orig_line.size(); ++p) {
                EXPECT_NEAR(loaded_line[p].x, orig_line[p].x,
                            std::max(tolerance, std::abs(orig_line[p].x) * 1e-4F))
                        << "X mismatch at frame " << frame.getValue()
                        << " line " << line_idx << " point " << p;
                EXPECT_NEAR(loaded_line[p].y, orig_line[p].y,
                            std::max(tolerance, std::abs(orig_line[p].y) * 1e-4F))
                        << "Y mismatch at frame " << frame.getValue()
                        << " line " << line_idx << " point " << p;
            }
        }
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvLineSingleRoundTripFuzz, FuzzCsvLineSingleRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 10000)).WithMinSize(1).WithMaxSize(50),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(1000),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(1000),
                fuzztest::InRange(1, 20));

}// namespace
