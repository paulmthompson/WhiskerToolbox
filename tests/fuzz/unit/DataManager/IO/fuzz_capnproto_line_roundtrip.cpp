/**
 * @file fuzz_capnproto_line_roundtrip.cpp
 * @brief Fuzz tests for CapnProto LineData round-trip (save -> load -> compare)
 *
 * Verifies that saving LineData to a CapnProto binary file and loading it back
 * produces identical data (binary format is lossless — no floating-point formatting loss).
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "IO/formats/CapnProto/linedata/Line_Data_Binary.hpp"
#include "Lines/Line_Data.hpp"

#include <cmath>
#include <filesystem>
#include <map>
#include <vector>

namespace {

/**
 * @brief Round-trip fuzz test: generate LineData, save as CapnProto binary, load back, compare.
 *
 * Builds LineData from fuzzed frame indices and coordinate vectors.
 * Because CapnProto is a binary format, the round-trip should be exact (no precision loss).
 *
 * @param frame_indices  Frame numbers for each line (clamped to non-negative).
 * @param x_coords       Flat X coordinates for all lines (split into groups by points_per_line).
 * @param y_coords       Flat Y coordinates for all lines.
 * @param points_per_line Number of points per line (1-20).
 */
void FuzzCapnProtoLineRoundTrip(
        std::vector<int> const & frame_indices,
        std::vector<float> const & x_coords,
        std::vector<float> const & y_coords,
        int points_per_line) {

    if (frame_indices.empty() || x_coords.empty() || y_coords.empty()) {
        return;
    }

    points_per_line = std::clamp(points_per_line, 1, 20);

    // Skip non-finite coordinates
    for (auto const & x: x_coords) {
        if (!std::isfinite(x)) return;
    }
    for (auto const & y: y_coords) {
        if (!std::isfinite(y)) return;
    }

    // Build LineData: each frame gets one line with points_per_line points
    auto const coord_count = std::min(x_coords.size(), y_coords.size());
    auto const num_lines = std::min(
            frame_indices.size(),
            coord_count / static_cast<size_t>(points_per_line));
    if (num_lines == 0) {
        return;
    }

    std::map<TimeFrameIndex, std::vector<Line2D>> input_map;
    for (size_t i = 0; i < num_lines; ++i) {
        int const frame = std::abs(frame_indices[i]);
        std::vector<Point2D<float>> points;
        points.reserve(static_cast<size_t>(points_per_line));
        for (int p = 0; p < points_per_line; ++p) {
            size_t const idx = i * static_cast<size_t>(points_per_line) + static_cast<size_t>(p);
            points.emplace_back(x_coords[idx], y_coords[idx]);
        }
        input_map[TimeFrameIndex(frame)].emplace_back(std::move(points));
    }

    if (input_map.empty()) {
        return;
    }

    LineData const line_data(input_map);

    // Save to temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_capnproto_line";
    std::filesystem::create_directories(temp_dir);

    BinaryLineSaverOptions save_opts;
    save_opts.filename = "fuzz_lines.capnp";
    save_opts.parent_dir = temp_dir.string();

    bool const save_ok = save(line_data, save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back
    BinaryLineLoaderOptions load_opts;
    load_opts.file_path = (temp_dir / "fuzz_lines.capnp").string();

    auto loaded = ::load(load_opts);
    ASSERT_NE(loaded, nullptr);

    // Compare: same number of frames
    auto const orig_times = line_data.getTimesWithData();
    auto const loaded_times = loaded->getTimesWithData();

    std::vector<TimeFrameIndex> orig_times_vec(orig_times.begin(), orig_times.end());
    std::vector<TimeFrameIndex> loaded_times_vec(loaded_times.begin(), loaded_times.end());

    std::sort(orig_times_vec.begin(), orig_times_vec.end());
    std::sort(loaded_times_vec.begin(), loaded_times_vec.end());

    ASSERT_EQ(orig_times_vec.size(), loaded_times_vec.size());

    // Binary format is lossless — compare exactly
    for (size_t t = 0; t < orig_times_vec.size(); ++t) {
        ASSERT_EQ(orig_times_vec[t], loaded_times_vec[t]);

        TimeFrameIndex const time = orig_times_vec[t];
        auto const & orig_lines = line_data.getAtTime(time);
        auto const & loaded_lines = loaded->getAtTime(time);

        ASSERT_EQ(loaded_lines.size(), orig_lines.size())
                << "Line count mismatch at frame " << time.getValue();

        for (size_t line_idx = 0; line_idx < orig_lines.size(); ++line_idx) {
            auto const & orig_line = orig_lines[line_idx];
            auto const & loaded_line = loaded_lines[line_idx];

            ASSERT_EQ(loaded_line.size(), orig_line.size())
                    << "Point count mismatch at frame " << time.getValue()
                    << " line " << line_idx;

            for (size_t p = 0; p < orig_line.size(); ++p) {
                EXPECT_FLOAT_EQ(loaded_line[p].x, orig_line[p].x)
                        << "X mismatch at frame " << time.getValue()
                        << " line " << line_idx << " point " << p;
                EXPECT_FLOAT_EQ(loaded_line[p].y, orig_line[p].y)
                        << "Y mismatch at frame " << time.getValue()
                        << " line " << line_idx << " point " << p;
            }
        }
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CapnProtoLineRoundTripFuzz, FuzzCapnProtoLineRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 10000)).WithMinSize(1).WithMaxSize(50),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(1000),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(1000),
                fuzztest::InRange(1, 20));

}// namespace
