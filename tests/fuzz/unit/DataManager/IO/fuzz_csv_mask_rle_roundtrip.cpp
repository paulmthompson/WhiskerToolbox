/**
 * @file fuzz_csv_mask_rle_roundtrip.cpp
 * @brief Fuzz tests for CSV MaskData RLE round-trip (save -> load -> compare)
 *
 * Verifies that saving MaskData with RLE encoding to CSV and loading it back
 * produces identical data (same frame count, same mask pixels per frame).
 * RLE encoding is lossless, so exact matches are expected.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "IO/formats/CSV/mask/Mask_Data_CSV.hpp"
#include "Masks/Mask_Data.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace {

/**
 * @brief Round-trip fuzz test: generate MaskData, save as CSV RLE, load back, compare.
 *
 * @param frame_indices  Fuzzed frame indices (one mask per frame).
 * @param pixel_coords   Fuzzed pixel coordinate pairs (x, y) to form masks.
 *                        Consumed sequentially across frames.
 * @param pixels_per_mask Number of pixels in each mask (one per frame index).
 */
void FuzzCsvMaskRleRoundTrip(
        std::vector<int> const & frame_indices,
        std::vector<std::pair<int, int>> const & pixel_coords,
        std::vector<int> const & pixels_per_mask) {

    if (frame_indices.empty() || pixel_coords.empty() || pixels_per_mask.empty()) {
        return;
    }

    // Build MaskData from fuzzed inputs
    auto mask_data = std::make_shared<MaskData>();

    // Deduplicate frame indices and make non-negative
    std::set<int> unique_frames(frame_indices.begin(), frame_indices.end());

    size_t coord_idx = 0;
    size_t mask_idx = 0;
    for (auto const frame: unique_frames) {
        if (frame < 0) {
            continue;
        }
        if (mask_idx >= pixels_per_mask.size()) {
            break;
        }

        int const n_pixels = std::clamp(std::abs(pixels_per_mask[mask_idx]), 1, 100);
        ++mask_idx;

        std::vector<Point2D<uint32_t>> points;
        // Use a set to deduplicate pixel coordinates within a mask
        std::set<std::pair<uint32_t, uint32_t>> seen_coords;
        for (int p = 0; p < n_pixels && coord_idx < pixel_coords.size(); ++p, ++coord_idx) {
            auto const x = static_cast<uint32_t>(std::abs(pixel_coords[coord_idx].first) % 1000);
            auto const y = static_cast<uint32_t>(std::abs(pixel_coords[coord_idx].second) % 1000);
            if (seen_coords.insert({x, y}).second) {
                points.emplace_back(x, y);
            }
        }

        if (points.empty()) {
            continue;
        }

        mask_data->addAtTime(TimeFrameIndex(frame), Mask2D(std::move(points)), NotifyObservers::No);
    }

    // Skip if no masks were generated
    auto const flattened = mask_data->flattened_data();
    if (std::ranges::empty(flattened)) {
        return;
    }

    // Save to a temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_mask_rle";
    std::filesystem::create_directories(temp_dir);

    CSVMaskRLESaverOptions save_opts;
    save_opts.filename = "fuzz_mask_rle.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.rle_delimiter = ",";
    save_opts.save_header = true;
    save_opts.header = "Frame,RLE";

    bool const save_ok = save(mask_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back with matching settings
    CSVMaskRLELoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_mask_rle.csv").string();
    load_opts.delimiter = ",";
    load_opts.rle_delimiter = ",";
    load_opts.has_header = true;
    load_opts.header_identifier = "Frame";

    auto loaded_map = load(load_opts);

    // Collect original frames and their masks for comparison
    std::map<int64_t, std::vector<Point2D<uint32_t>>> original_masks;
    for (auto const & [time, entity_id, mask]: mask_data->flattened_data()) {
        auto pts = mask.points();
        auto cmp = [](auto const & a, auto const & b) {
            return (a.y < b.y) || (a.y == b.y && a.x < b.x);
        };
        std::sort(pts.begin(), pts.end(), cmp);
        for (auto const & pt: pts) {
            original_masks[time.getValue()].push_back(pt);
        }
    }

    // Compare: same frame count
    ASSERT_EQ(loaded_map.size(), original_masks.size());

    // Compare: same pixels per frame (RLE is lossless)
    for (auto const & [frame_val, orig_pts]: original_masks) {
        auto const frame_idx = TimeFrameIndex(frame_val);
        ASSERT_TRUE(loaded_map.count(frame_idx) > 0)
                << "Missing frame " << frame_val << " in loaded data";

        auto const & loaded_masks = loaded_map[frame_idx];

        // Count total loaded pixels across all masks at this frame
        size_t loaded_pixel_count = 0;
        std::vector<Point2D<uint32_t>> loaded_pts;
        for (auto const & m: loaded_masks) {
            for (size_t i = 0; i < m.size(); ++i) {
                loaded_pts.push_back(m[i]);
            }
            loaded_pixel_count += m.size();
        }

        ASSERT_EQ(loaded_pixel_count, orig_pts.size())
                << "Pixel count mismatch at frame " << frame_val;

        // Sort loaded points for comparison
        auto cmp = [](auto const & a, auto const & b) {
            return (a.y < b.y) || (a.y == b.y && a.x < b.x);
        };
        std::sort(loaded_pts.begin(), loaded_pts.end(), cmp);

        for (size_t i = 0; i < orig_pts.size(); ++i) {
            EXPECT_EQ(loaded_pts[i].x, orig_pts[i].x)
                    << "X mismatch at frame " << frame_val << " pixel " << i;
            EXPECT_EQ(loaded_pts[i].y, orig_pts[i].y)
                    << "Y mismatch at frame " << frame_val << " pixel " << i;
        }
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvMaskRleRoundTripFuzz, FuzzCsvMaskRleRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 10000)).WithMinSize(1).WithMaxSize(50),
                fuzztest::VectorOf(fuzztest::PairOf(fuzztest::InRange(0, 999),
                                                    fuzztest::InRange(0, 999)))
                        .WithMinSize(1)
                        .WithMaxSize(1000),
                fuzztest::VectorOf(fuzztest::InRange(1, 50)).WithMinSize(1).WithMaxSize(50));

/**
 * @brief Fuzz test with varying RLE delimiter settings.
 *
 * Tests that save/load round-trip works with non-default RLE delimiters,
 * while keeping the column delimiter as comma.
 */
void FuzzCsvMaskRleRoundTripDelimiter(
        std::vector<int> const & frame_indices,
        std::vector<std::pair<int, int>> const & pixel_coords,
        std::string const & rle_delimiter) {

    if (frame_indices.empty() || pixel_coords.empty() || rle_delimiter.empty()) {
        return;
    }

    // Skip delimiters that would conflict with parsing
    for (char const c: rle_delimiter) {
        if (c == '\n' || c == '\r' || c == '"' ||
            std::isdigit(static_cast<unsigned char>(c))) {
            return;
        }
    }

    // Build a simple MaskData with one mask per frame
    auto mask_data = std::make_shared<MaskData>();

    std::set<int> unique_frames(frame_indices.begin(), frame_indices.end());
    size_t coord_idx = 0;

    for (auto const frame: unique_frames) {
        if (frame < 0) {
            continue;
        }

        int const n_pixels = std::min(static_cast<int>(pixel_coords.size() - coord_idx), 10);
        if (n_pixels <= 0) {
            break;
        }

        std::vector<Point2D<uint32_t>> points;
        std::set<std::pair<uint32_t, uint32_t>> seen_coords;
        for (int p = 0; p < n_pixels; ++p, ++coord_idx) {
            auto const x = static_cast<uint32_t>(std::abs(pixel_coords[coord_idx].first) % 500);
            auto const y = static_cast<uint32_t>(std::abs(pixel_coords[coord_idx].second) % 500);
            if (seen_coords.insert({x, y}).second) {
                points.emplace_back(x, y);
            }
        }

        if (!points.empty()) {
            mask_data->addAtTime(TimeFrameIndex(frame), Mask2D(std::move(points)), NotifyObservers::No);
        }
    }

    auto const flattened = mask_data->flattened_data();
    if (std::ranges::empty(flattened)) {
        return;
    }

    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_mask_rle_delim";
    std::filesystem::create_directories(temp_dir);

    CSVMaskRLESaverOptions save_opts;
    save_opts.filename = "fuzz_mask_rle_delim.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.rle_delimiter = rle_delimiter;
    save_opts.save_header = false;

    bool const save_ok = save(mask_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    CSVMaskRLELoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_mask_rle_delim.csv").string();
    load_opts.delimiter = ",";
    load_opts.rle_delimiter = rle_delimiter;
    load_opts.has_header = false;

    auto loaded_map = load(load_opts);

    // Collect original masks
    std::map<int64_t, size_t> original_pixel_counts;
    for (auto const & [time, entity_id, mask]: mask_data->flattened_data()) {
        original_pixel_counts[time.getValue()] += mask.size();
    }

    // Verify same number of frames
    if (rle_delimiter.size() == 1) {
        ASSERT_EQ(loaded_map.size(), original_pixel_counts.size());

        for (auto const & [frame_val, expected_count]: original_pixel_counts) {
            auto const frame_idx = TimeFrameIndex(frame_val);
            ASSERT_TRUE(loaded_map.count(frame_idx) > 0);

            size_t loaded_count = 0;
            for (auto const & m: loaded_map[frame_idx]) {
                loaded_count += m.size();
            }
            EXPECT_EQ(loaded_count, expected_count)
                    << "Pixel count mismatch at frame " << frame_val;
        }
    }

    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvMaskRleRoundTripFuzz, FuzzCsvMaskRleRoundTripDelimiter)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 5000)).WithMinSize(1).WithMaxSize(20),
                fuzztest::VectorOf(fuzztest::PairOf(fuzztest::InRange(0, 499),
                                                    fuzztest::InRange(0, 499)))
                        .WithMinSize(1)
                        .WithMaxSize(200),
                fuzztest::OneOf(fuzztest::Just(std::string(",")),
                                fuzztest::Just(std::string(" ")),
                                fuzztest::Just(std::string("\t")),
                                fuzztest::Just(std::string(";"))));

}// namespace
