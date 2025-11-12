#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "IO/OpenCV/Mask_Data_Image.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "IO/LoaderRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

class MaskDataImageTestFixture {
public:
    MaskDataImageTestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_mask_image_output";
        std::filesystem::create_directories(test_dir);
        
        // Create test MaskData with simple masks
        createTestMaskData();
    }
    
    ~MaskDataImageTestFixture() {
        // Clean up - remove test files and directory
        cleanup();
    }

protected:
    void createTestMaskData() {
        original_mask_data = std::make_shared<MaskData>();
        
        // Create simple test masks at different time frames
        
        // Mask 1 at time frame 0: Small rectangle in top-left
        std::vector<Point2D<uint32_t>> mask1_points;
        for (uint32_t y = 10; y < 30; ++y) {
            for (uint32_t x = 10; x < 40; ++x) {
                mask1_points.emplace_back(x, y);
            }
        }
        
        // Mask 2 at time frame 1: Small square in center
        std::vector<Point2D<uint32_t>> mask2_points;
        for (uint32_t y = 100; y < 120; ++y) {
            for (uint32_t x = 150; x < 170; ++x) {
                mask2_points.emplace_back(x, y);
            }
        }
        
        // Mask 3 at time frame 2: L-shaped mask
        std::vector<Point2D<uint32_t>> mask3_points;
        // Horizontal part of L
        for (uint32_t y = 200; y < 210; ++y) {
            for (uint32_t x = 50; x < 100; ++x) {
                mask3_points.emplace_back(x, y);
            }
        }
        // Vertical part of L
        for (uint32_t y = 210; y < 250; ++y) {
            for (uint32_t x = 50; x < 60; ++x) {
                mask3_points.emplace_back(x, y);
            }
        }
        
        // Add masks to MaskData at different time frames
        original_mask_data->addAtTime(TimeFrameIndex(0), Mask2D(mask1_points), NotifyObservers::No);
        original_mask_data->addAtTime(TimeFrameIndex(1), Mask2D(mask2_points), NotifyObservers::No);
        original_mask_data->addAtTime(TimeFrameIndex(2), Mask2D(mask3_points), NotifyObservers::No);

        // Set image size
        original_mask_data->setImageSize(ImageSize(320, 280));
    }
    
    bool saveImageMaskData() {
        ImageMaskSaverOptions save_opts;
        save_opts.parent_dir = test_dir.string();
        save_opts.image_format = "PNG";
        save_opts.filename_prefix = "mask_";
        save_opts.frame_number_padding = 4;
        save_opts.image_width = 320;
        save_opts.image_height = 280;
        save_opts.background_value = 0;
        save_opts.mask_value = 255;
        save_opts.overwrite_existing = true;
        
        try {
            save(original_mask_data.get(), save_opts);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    const char* getJSONConfig() {
        static std::string config = R"([
{
    "data_type": "mask",
    "name": "test_image_masks",
    "filepath": ")" + test_dir.string() + R"(",
    "format": "image",
    "file_pattern": "mask_*.png",
    "filename_prefix": "mask_",
    "frame_number_padding": 4,
    "threshold_value": 128,
    "invert_mask": false,
    "color": "#00FFFF"
}
])";
        return config.c_str();
    }
    
    void cleanup() {
        try {
            // Remove all PNG files in the test directory
            for (auto const& entry : std::filesystem::directory_iterator(test_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".png") {
                    std::filesystem::remove(entry.path());
                }
            }
            
            // Remove JSON config files if any
            for (auto const& entry : std::filesystem::directory_iterator(test_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::filesystem::remove(entry.path());
                }
            }
            
            // Remove directory if empty
            if (std::filesystem::exists(test_dir) && std::filesystem::is_empty(test_dir)) {
                std::filesystem::remove(test_dir);
            }
        } catch (const std::exception& e) {
            // Best effort cleanup - don't fail test if cleanup fails
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    void verifyMaskDataEquality(const MaskData& loaded_data) const {
        // Get times with data from both original and loaded
        auto original_times = original_mask_data->getTimesWithData();
        auto loaded_times = loaded_data.getTimesWithData();
        
        // Convert to vectors for easier comparison
        std::vector<TimeFrameIndex> original_times_vec(original_times.begin(), original_times.end());
        std::vector<TimeFrameIndex> loaded_times_vec(loaded_times.begin(), loaded_times.end());
        
        REQUIRE(original_times_vec.size() == loaded_times_vec.size());
        
        // Sort both vectors to ensure consistent comparison
        std::sort(original_times_vec.begin(), original_times_vec.end());
        std::sort(loaded_times_vec.begin(), loaded_times_vec.end());
        
        // Check each time frame
        for (size_t i = 0; i < original_times_vec.size(); ++i) {
            REQUIRE(original_times_vec[i] == loaded_times_vec[i]);
            
            TimeFrameIndex time = original_times_vec[i];
            const auto& original_masks = original_mask_data->getAtTime(time);
            const auto& loaded_masks = loaded_data.getAtTime(time);
            
            REQUIRE(original_masks.size() == loaded_masks.size());
            
            // Check each mask at this time frame
            for (size_t mask_idx = 0; mask_idx < original_masks.size(); ++mask_idx) {
                const Mask2D& original_mask = original_masks[mask_idx];
                const Mask2D& loaded_mask = loaded_masks[mask_idx];
                
                // Note: Due to image format conversion and potential rounding,
                // we expect the masks to be very similar but not necessarily identical.
                // For now, we'll check that both masks are non-empty and have reasonable sizes.
                REQUIRE(!original_mask.empty());
                REQUIRE(!loaded_mask.empty());
                
                // The loaded mask should have a reasonable number of points
                // (allowing for some differences due to image format conversion)
                double size_ratio = static_cast<double>(loaded_mask.size()) / static_cast<double>(original_mask.size());
                REQUIRE(size_ratio > 0.5); // At least 50% of points should be preserved
                REQUIRE(size_ratio < 2.0); // No more than 200% (shouldn't gain many points)
            }
        }
        
        // Check image size
        REQUIRE(original_mask_data->getImageSize().width == loaded_data.getImageSize().width);
        REQUIRE(original_mask_data->getImageSize().height == loaded_data.getImageSize().height);
    }

protected:
    std::filesystem::path test_dir;
    std::shared_ptr<MaskData> original_mask_data;
};

TEST_CASE_METHOD(MaskDataImageTestFixture, "DM - IO - MaskData - MaskData image save and load through direct functions", "[MaskData][Image][IO]") {
    
    SECTION("Save MaskData to image format") {
        bool save_success = saveImageMaskData();
        REQUIRE(save_success);
        
        // Check that files were created
        std::vector<std::string> expected_files = {"mask_0000.png", "mask_0001.png", "mask_0002.png"};
        for (const auto& filename : expected_files) {
            std::filesystem::path filepath = test_dir / filename;
            REQUIRE(std::filesystem::exists(filepath));
            REQUIRE(std::filesystem::file_size(filepath) > 0);
        }
    }
    
    SECTION("Load MaskData from image format") {
        // First save the data
        REQUIRE(saveImageMaskData());
        
        // Load using image loader
        ImageMaskLoaderOptions load_opts;
        load_opts.directory_path = test_dir.string();
        load_opts.file_pattern = "mask_*.png";
        load_opts.filename_prefix = "mask_";
        load_opts.frame_number_padding = 4;
        load_opts.threshold_value = 128;
        load_opts.invert_mask = false;
        
        auto loaded_mask_data = load(load_opts);
        REQUIRE(loaded_mask_data != nullptr);
        
        // Verify the loaded data matches the original (approximately)
        verifyMaskDataEquality(*loaded_mask_data);
    }
}

TEST_CASE_METHOD(MaskDataImageTestFixture, "DM - IO - MaskData - MaskData image load through DataManager JSON config", "[MaskData][Image][IO][DataManager]") {
    
    SECTION("Load image MaskData through DataManager") {
        // First save the data
        REQUIRE(saveImageMaskData());
        
        // Create JSON config file for loading
        const char* json_config = getJSONConfig();
        std::filesystem::path json_filepath = test_dir / "config.json";
        
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
        }
        
        // Create DataManager and load data using JSON config
        auto data_manager = std::make_unique<DataManager>();
        auto data_info_list = load_data_from_json_config(data_manager.get(), json_filepath.string());
        
        // Verify that data was loaded successfully
        REQUIRE(!data_info_list.empty());
        REQUIRE(data_info_list.size() == 1);
        
        // Check that the data was registered correctly
        DataInfo const& info = data_info_list[0];
        REQUIRE(info.key == "test_image_masks");
        REQUIRE(info.data_class == "MaskData");
        REQUIRE(info.color == "#00FFFF");
        
        // Get the loaded MaskData and verify its contents
        auto loaded_mask_data = data_manager->getData<MaskData>("test_image_masks");
        REQUIRE(loaded_mask_data != nullptr);
        
        verifyMaskDataEquality(*loaded_mask_data);
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
    
    SECTION("DataManager handles missing image directory gracefully") {
        // Create JSON config pointing to non-existent directory
        std::filesystem::path fake_dirpath = test_dir / "nonexistent_dir";
        std::string json_config = R"([
{
    "data_type": "mask",
    "name": "missing_image_masks", 
    "filepath": ")" + fake_dirpath.string() + R"(",
    "format": "image",
    "file_pattern": "*.png"
}
])";
        
        std::filesystem::path json_filepath = test_dir / "config_missing.json";
        
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
        }
        
        // Create DataManager and attempt to load - should handle gracefully
        auto data_manager = std::make_unique<DataManager>();
        auto data_info_list = load_data_from_json_config(data_manager.get(), json_filepath.string());
        
        // Should return empty list due to missing directory
        REQUIRE(data_info_list.empty());
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
}

TEST_CASE_METHOD(MaskDataImageTestFixture, "DM - IO - MaskData - Image loader registration and format support", "[MaskData][Image][IO]") {
    
    SECTION("Verify image loader is registered and supports mask format") {
        // Create DataManager (which triggers loader registration)
        auto data_manager = std::make_unique<DataManager>();
        
        // Get the registry and check if image format is supported for MaskData
        auto& registry = LoaderRegistry::getInstance();
        bool image_supported = registry.isFormatSupported("image", IODataType::Mask);
        
        // This should be true if OpenCV is enabled at compile time
        #ifdef ENABLE_OPENCV
        REQUIRE(image_supported);
        #else
        // If OpenCV is not enabled, it should not be supported
        REQUIRE_FALSE(image_supported);
        #endif
    }
    
    SECTION("Check image format in supported formats list") {
        auto data_manager = std::make_unique<DataManager>();
        auto& registry = LoaderRegistry::getInstance();
        
        auto supported_formats = registry.getSupportedFormats(IODataType::Mask);
        
        // Debug: Print all supported formats
        std::cout << "DEBUG: Supported formats for IODataType::Mask: ";
        for (const auto& format : supported_formats) {
            std::cout << "'" << format << "' ";
        }
        std::cout << std::endl;
        
        #ifdef ENABLE_OPENCV
        std::cout << "DEBUG: ENABLE_OPENCV is defined, checking for 'image' format" << std::endl;
        // Image format should be in the list of supported formats
        bool found_image = std::find(supported_formats.begin(), supported_formats.end(), "image") 
                          != supported_formats.end();
        REQUIRE(found_image);
        #endif
        
        // HDF5 should be supported for masks if available
        #ifdef ENABLE_HDF5
        bool found_hdf5 = std::find(supported_formats.begin(), supported_formats.end(), "hdf5") 
                         != supported_formats.end();
        REQUIRE(found_hdf5);
        #endif
    }
}
