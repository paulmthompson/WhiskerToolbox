/**
 * @file fuzz_opencv_mask_save.cpp
 * @brief Fuzz tests for OpenCV MaskData image saver robustness
 *
 * Verifies that saving MaskData to image files with fuzzed options does not
 * crash and produces valid output files. No round-trip verification is
 * performed because the image format introduces pixel-level quantization
 * (mask coordinates -> rasterized image -> re-extracted coordinates).
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "IO/formats/OpenCV/maskdata/Mask_Data_Image.hpp"
#include "Masks/Mask_Data.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

namespace {

/**
 * @brief Robustness fuzz test: generate MaskData, save as image files, verify no crash
 *        and output files exist with non-zero size.
 *
 * @param frame_indices   Fuzzed frame indices (one mask per unique frame).
 * @param pixel_coords    Fuzzed pixel coordinate pairs (x, y) to form masks.
 * @param pixels_per_mask Number of pixels in each mask (one per frame index).
 * @param image_width     Fuzzed output image width.
 * @param image_height    Fuzzed output image height.
 */
void FuzzOpenCVMaskSave(
        std::vector<int> const & frame_indices,
        std::vector<std::pair<int, int>> const & pixel_coords,
        std::vector<int> const & pixels_per_mask,
        int image_width,
        int image_height) {

    if (frame_indices.empty() || pixel_coords.empty() || pixels_per_mask.empty()) {
        return;
    }

    // Build MaskData from fuzzed inputs
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize(ImageSize{image_width, image_height});

    // Deduplicate frame indices and make non-negative
    std::set<int> unique_frames(frame_indices.begin(), frame_indices.end());

    size_t coord_idx = 0;
    size_t mask_idx = 0;
    int frames_added = 0;
    for (auto const frame: unique_frames) {
        if (frame < 0) {
            continue;
        }
        if (mask_idx >= pixels_per_mask.size()) {
            break;
        }

        int const n_pixels = std::clamp(std::abs(pixels_per_mask[mask_idx]), 1, 50);
        ++mask_idx;

        std::vector<Point2D<uint32_t>> points;
        std::set<std::pair<uint32_t, uint32_t>> seen_coords;
        for (int p = 0; p < n_pixels && coord_idx < pixel_coords.size(); ++p, ++coord_idx) {
            auto const x = static_cast<uint32_t>(
                    std::abs(pixel_coords[coord_idx].first) % image_width);
            auto const y = static_cast<uint32_t>(
                    std::abs(pixel_coords[coord_idx].second) % image_height);
            if (seen_coords.insert({x, y}).second) {
                points.emplace_back(x, y);
            }
        }

        if (points.empty()) {
            continue;
        }

        mask_data->addAtTime(TimeFrameIndex(frame), Mask2D(std::move(points)), NotifyObservers::No);
        ++frames_added;
    }

    if (frames_added == 0) {
        return;
    }

    // Save to a temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_opencv_mask";
    std::filesystem::create_directories(temp_dir);

    ImageMaskSaverOptions save_opts;
    save_opts.parent_dir = temp_dir.string();
    save_opts.image_format = "PNG";
    save_opts.filename_prefix = "mask_";
    save_opts.frame_number_padding = 4;
    save_opts.image_width = image_width;
    save_opts.image_height = image_height;
    save_opts.background_value = 0;
    save_opts.mask_value = 255;
    save_opts.overwrite_existing = true;

    bool const save_ok = save(mask_data.get(), save_opts);

    if (save_ok) {
        // Verify output files exist and have non-zero size
        int files_found = 0;
        for (auto const & entry: std::filesystem::directory_iterator(temp_dir)) {
            if (entry.is_regular_file()) {
                auto const ext = entry.path().extension().string();
                if (ext == ".png") {
                    EXPECT_GT(entry.file_size(), 0u)
                            << "Image file has zero size: " << entry.path().string();
                    ++files_found;
                }
            }
        }
        EXPECT_EQ(files_found, frames_added)
                << "Expected " << frames_added << " image files, found " << files_found;
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(OpenCVMaskSaveFuzz, FuzzOpenCVMaskSave)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 1000)).WithMinSize(1).WithMaxSize(10),
                fuzztest::VectorOf(fuzztest::PairOf(fuzztest::InRange(0, 639),
                                                    fuzztest::InRange(0, 479)))
                        .WithMinSize(1)
                        .WithMaxSize(200),
                fuzztest::VectorOf(fuzztest::InRange(1, 20)).WithMinSize(1).WithMaxSize(10),
                fuzztest::InRange(16, 640),
                fuzztest::InRange(16, 480));

/**
 * @brief Fuzz test with varying image formats (PNG, BMP).
 *
 * Only tests formats that OpenCV reliably supports without external codecs.
 */
void FuzzOpenCVMaskSaveFormats(
        std::vector<int> const & frame_indices,
        std::vector<std::pair<int, int>> const & pixel_coords,
        std::string const & image_format) {

    if (frame_indices.empty() || pixel_coords.empty() || image_format.empty()) {
        return;
    }

    // Build simple MaskData (one mask per frame, 5 pixels each)
    auto mask_data = std::make_shared<MaskData>();
    int const img_w = 64;
    int const img_h = 64;
    mask_data->setImageSize(ImageSize{img_w, img_h});

    std::set<int> unique_frames(frame_indices.begin(), frame_indices.end());
    size_t coord_idx = 0;
    int frames_added = 0;

    for (auto const frame: unique_frames) {
        if (frame < 0) {
            continue;
        }

        std::vector<Point2D<uint32_t>> points;
        std::set<std::pair<uint32_t, uint32_t>> seen;
        for (int p = 0; p < 5 && coord_idx < pixel_coords.size(); ++p, ++coord_idx) {
            auto const x = static_cast<uint32_t>(
                    std::abs(pixel_coords[coord_idx].first) % img_w);
            auto const y = static_cast<uint32_t>(
                    std::abs(pixel_coords[coord_idx].second) % img_h);
            if (seen.insert({x, y}).second) {
                points.emplace_back(x, y);
            }
        }

        if (points.empty()) {
            continue;
        }

        mask_data->addAtTime(TimeFrameIndex(frame), Mask2D(std::move(points)), NotifyObservers::No);
        ++frames_added;
    }

    if (frames_added == 0) {
        return;
    }

    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_opencv_mask_fmt";
    std::filesystem::create_directories(temp_dir);

    ImageMaskSaverOptions save_opts;
    save_opts.parent_dir = temp_dir.string();
    save_opts.image_format = image_format;
    save_opts.image_width = img_w;
    save_opts.image_height = img_h;
    save_opts.overwrite_existing = true;

    bool const save_ok = save(mask_data.get(), save_opts);

    if (save_ok) {
        // Verify files exist
        int files_found = 0;
        for (auto const & entry: std::filesystem::directory_iterator(temp_dir)) {
            if (entry.is_regular_file() && entry.file_size() > 0) {
                ++files_found;
            }
        }
        EXPECT_EQ(files_found, frames_added);
    }

    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(OpenCVMaskSaveFuzz, FuzzOpenCVMaskSaveFormats)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100)).WithMinSize(1).WithMaxSize(5),
                fuzztest::VectorOf(fuzztest::PairOf(fuzztest::InRange(0, 63),
                                                    fuzztest::InRange(0, 63)))
                        .WithMinSize(5)
                        .WithMaxSize(50),
                fuzztest::ElementOf<std::string>({"PNG", "BMP", "TIFF"}));

}// namespace
