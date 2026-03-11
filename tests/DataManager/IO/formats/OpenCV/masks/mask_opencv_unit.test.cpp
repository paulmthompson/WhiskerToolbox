/**
 * @file mask_opencv_unit.test.cpp
 * @brief Deterministic unit tests for MaskData OpenCV image save
 *
 * Tests include:
 * 1. Single mask save produces a file
 * 2. Multiple frames produce correct number of files
 * 3. Empty MaskData save returns true with no files
 * 4. Save returns true on success
 * 5. Filename prefix and padding are applied correctly
 * 6. Overwrite protection works
 */

#include <catch2/catch_test_macros.hpp>

#include "IO/formats/OpenCV/maskdata/Mask_Data_Image.hpp"
#include "Masks/Mask_Data.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class MaskOpenCVUnitTestFixture {
public:
    MaskOpenCVUnitTestFixture() {
        test_dir = std::filesystem::current_path() / "test_mask_opencv_unit_output";
        std::filesystem::create_directories(test_dir);
    }

    ~MaskOpenCVUnitTestFixture() {
        cleanup();
    }

protected:
    void cleanup() {
        try {
            if (std::filesystem::exists(test_dir)) {
                std::filesystem::remove_all(test_dir);
            }
        } catch (std::exception const & e) {
            static_cast<void>(e);
        }
    }

    [[nodiscard]] ImageMaskSaverOptions defaultSaveOpts() const {
        ImageMaskSaverOptions opts;
        opts.parent_dir = test_dir.string();
        opts.image_format = "PNG";
        opts.filename_prefix = "mask_";
        opts.frame_number_padding = 4;
        opts.image_width = 64;
        opts.image_height = 64;
        opts.background_value = 0;
        opts.mask_value = 255;
        opts.overwrite_existing = true;
        return opts;
    }

    /// Count PNG files in the test directory
    [[nodiscard]] int countImageFiles(std::string const & extension = ".png") const {
        int count = 0;
        for (auto const & entry: std::filesystem::directory_iterator(test_dir)) {
            if (entry.is_regular_file() && entry.path().extension().string() == extension) {
                ++count;
            }
        }
        return count;
    }

protected:
    std::filesystem::path test_dir;
};

TEST_CASE_METHOD(MaskOpenCVUnitTestFixture,
                 "DM - IO - MaskData - OpenCV image save",
                 "[MaskData][OpenCV][IO][unit]") {

    SECTION("Single mask produces one image file") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(ImageSize{64, 64});
        Mask2D mask({{10, 5}, {11, 5}, {12, 5}});
        mask_data->addAtTime(TimeFrameIndex(0), std::move(mask), NotifyObservers::No);

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);
        CHECK(countImageFiles() == 1);

        // Verify the specific file exists
        auto expected_path = test_dir / "mask_0000.png";
        CHECK(std::filesystem::exists(expected_path));
        CHECK(std::filesystem::file_size(expected_path) > 0);
    }

    SECTION("Multiple frames produce correct number of files") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(ImageSize{64, 64});

        Mask2D mask0({{0, 0}, {1, 0}});
        mask_data->addAtTime(TimeFrameIndex(0), std::move(mask0), NotifyObservers::No);

        Mask2D mask5({{5, 10}, {6, 10}});
        mask_data->addAtTime(TimeFrameIndex(5), std::move(mask5), NotifyObservers::No);

        Mask2D mask42({{20, 30}});
        mask_data->addAtTime(TimeFrameIndex(42), std::move(mask42), NotifyObservers::No);

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);
        CHECK(countImageFiles() == 3);

        CHECK(std::filesystem::exists(test_dir / "mask_0000.png"));
        CHECK(std::filesystem::exists(test_dir / "mask_0005.png"));
        CHECK(std::filesystem::exists(test_dir / "mask_0042.png"));
    }

    SECTION("Empty MaskData save returns true with no files") {
        auto mask_data = std::make_shared<MaskData>();

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);
        CHECK(countImageFiles() == 0);
    }

    SECTION("Null pointer returns false") {
        bool const save_ok = save(nullptr, defaultSaveOpts());
        CHECK_FALSE(save_ok);
    }

    SECTION("Custom prefix and padding") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(ImageSize{64, 64});
        Mask2D mask({{10, 10}});
        mask_data->addAtTime(TimeFrameIndex(7), std::move(mask), NotifyObservers::No);

        auto opts = defaultSaveOpts();
        opts.filename_prefix = "frame";
        opts.frame_number_padding = 6;

        bool const save_ok = save(mask_data.get(), opts);
        REQUIRE(save_ok);
        CHECK(std::filesystem::exists(test_dir / "frame000007.png"));
    }

    SECTION("Overwrite protection skips existing files") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(ImageSize{64, 64});
        Mask2D mask({{10, 10}});
        mask_data->addAtTime(TimeFrameIndex(0), std::move(mask), NotifyObservers::No);

        // First save
        auto opts = defaultSaveOpts();
        opts.overwrite_existing = false;
        bool save_ok = save(mask_data.get(), opts);
        REQUIRE(save_ok);
        CHECK(countImageFiles() == 1);

        auto const file_path = test_dir / "mask_0000.png";
        auto const original_size = std::filesystem::file_size(file_path);
        auto const original_time = std::filesystem::last_write_time(file_path);

        // Second save with different mask data but same frame - should skip
        auto mask_data2 = std::make_shared<MaskData>();
        mask_data2->setImageSize(ImageSize{64, 64});
        Mask2D mask2({{30, 30}, {31, 30}, {32, 30}, {33, 30}});
        mask_data2->addAtTime(TimeFrameIndex(0), std::move(mask2), NotifyObservers::No);

        // With overwrite_existing = false, existing file is skipped (not an error)
        save_ok = save(mask_data2.get(), opts);
        // File should be unchanged
        CHECK(std::filesystem::file_size(file_path) == original_size);
        CHECK(std::filesystem::last_write_time(file_path) == original_time);
    }

    SECTION("BMP format produces valid files") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize(ImageSize{32, 32});
        Mask2D mask({{5, 5}, {6, 5}});
        mask_data->addAtTime(TimeFrameIndex(1), std::move(mask), NotifyObservers::No);

        auto opts = defaultSaveOpts();
        opts.image_format = "BMP";

        bool const save_ok = save(mask_data.get(), opts);
        REQUIRE(save_ok);
        CHECK(countImageFiles(".bmp") == 1);
        CHECK(std::filesystem::exists(test_dir / "mask_0001.bmp"));
        CHECK(std::filesystem::file_size(test_dir / "mask_0001.bmp") > 0);
    }
}
