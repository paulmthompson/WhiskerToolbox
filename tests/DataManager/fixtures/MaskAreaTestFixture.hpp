#ifndef MASK_AREA_TEST_FIXTURE_HPP
#define MASK_AREA_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <string>
#include <map>

/**
 * @brief Test fixture for MaskArea transform tests
 * 
 * This fixture provides pre-populated MaskData objects for testing
 * the mask area calculation transform in both V1 and V2 implementations.
 * 
 * Test data scenarios:
 * - empty_mask_data: No masks (tests empty input handling)
 * - single_mask_single_timestamp: One mask at one time (basic case)
 * - multiple_masks_single_timestamp: Multiple masks summed at one time
 * - single_masks_multiple_timestamps: One mask per timestamp across time
 * - mixed_masks_multiple_timestamps: Varying mask counts across time
 * - empty_mask_at_timestamp: A mask with zero pixels
 * - mixed_empty_nonempty: Empty and non-empty masks at same timestamp
 * - large_mask_count: Many masks at one timestamp (stress test)
 * - json_pipeline_basic: Two masks at different timestamps for JSON tests
 * - json_pipeline_multi_timestamp: Three timestamps for comprehensive JSON tests
 * - json_pipeline_multi_mask: Multiple masks at same timestamp for JSON tests
 */
class MaskAreaTestFixture {
protected:
    MaskAreaTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    ~MaskAreaTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    std::shared_ptr<DataManager> getSharedDataManager() {
        return std::shared_ptr<DataManager>(m_data_manager.get(), [](DataManager*){});
    }

    /// Map of named test data objects for direct access
    std::map<std::string, std::shared_ptr<MaskData>> m_test_masks;

private:
    void populateTestData() {
        // =================================================================
        // Core functionality tests
        // =================================================================
        
        // Empty mask data - no masks at all
        // V1 Expected: empty AnalogTimeSeries
        // V2 Expected: empty RaggedAnalogTimeSeries
        createEmptyMaskData("empty_mask_data");
        
        // Single mask at single timestamp (3 pixels)
        // V1 Expected: {10: 3.0}
        // V2 Expected: {10: [3.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> x = {1, 2, 3};
            std::vector<uint32_t> y = {1, 2, 3};
            mask_data->addAtTime(TimeFrameIndex(10), Mask2D(x, y), NotifyObservers::No);
            storeMaskData("single_mask_single_timestamp", mask_data);
        }
        
        // Multiple masks at single timestamp (3 + 5 = 8 pixels)
        // V1 Expected: {20: 8.0} (summed)
        // V2 Expected: {20: [3.0, 5.0]} (individual)
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> x1 = {1, 2, 3};
            std::vector<uint32_t> y1 = {1, 2, 3};
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(x1, y1), NotifyObservers::No);
            
            std::vector<uint32_t> x2 = {4, 5, 6, 7, 8};
            std::vector<uint32_t> y2 = {4, 5, 6, 7, 8};
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(x2, y2), NotifyObservers::No);
            storeMaskData("multiple_masks_single_timestamp", mask_data);
        }
        
        // Single masks across multiple timestamps
        // Timestamp 30: 2 pixels, Timestamp 40: 7 pixels (3+4)
        // V1 Expected: {30: 2.0, 40: 7.0}
        // V2 Expected: {30: [2.0], 40: [3.0, 4.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            // Timestamp 30: 2 pixels
            std::vector<uint32_t> x1 = {1, 2};
            std::vector<uint32_t> y1 = {1, 2};
            mask_data->addAtTime(TimeFrameIndex(30), Mask2D(x1, y1), NotifyObservers::No);
            
            // Timestamp 40: two masks (3 + 4 pixels)
            std::vector<uint32_t> x2 = {1, 2, 3};
            std::vector<uint32_t> y2 = {1, 2, 3};
            mask_data->addAtTime(TimeFrameIndex(40), Mask2D(x2, y2), NotifyObservers::No);
            
            std::vector<uint32_t> x3 = {4, 5, 6, 7};
            std::vector<uint32_t> y3 = {4, 5, 6, 7};
            mask_data->addAtTime(TimeFrameIndex(40), Mask2D(x3, y3), NotifyObservers::No);
            
            storeMaskData("masks_multiple_timestamps", mask_data);
        }
        
        // Single mask for statistics verification (4 pixels)
        // V1 Expected: mean=4.0, min=4.0, max=4.0
        // V2 Expected: {100: [4.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> x = {1, 2, 3, 4};
            std::vector<uint32_t> y = {1, 2, 3, 4};
            mask_data->addAtTime(TimeFrameIndex(100), Mask2D(x, y), NotifyObservers::No);
            storeMaskData("single_mask_for_statistics", mask_data);
        }
        
        // =================================================================
        // Edge cases
        // =================================================================
        
        // Empty mask (mask with zero pixels) at a timestamp
        // V1 Expected: {10: 0.0}
        // V2 Expected: {10: [0.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> empty_x;
            std::vector<uint32_t> empty_y;
            mask_data->addAtTime(TimeFrameIndex(10), Mask2D(empty_x, empty_y), NotifyObservers::No);
            storeMaskData("empty_mask_at_timestamp", mask_data);
        }
        
        // Mixed empty and non-empty masks at same timestamp
        // V1 Expected: {20: 3.0} (0 + 3)
        // V2 Expected: {20: [0.0, 3.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            // Empty mask
            std::vector<uint32_t> empty_x;
            std::vector<uint32_t> empty_y;
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(empty_x, empty_y), NotifyObservers::No);
            
            // Non-empty mask (3 pixels)
            std::vector<uint32_t> x = {1, 2, 3};
            std::vector<uint32_t> y = {1, 2, 3};
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(x, y), NotifyObservers::No);
            
            storeMaskData("mixed_empty_nonempty", mask_data);
        }
        
        // Large number of masks at one timestamp
        // 10 masks with 1, 2, 3, ..., 10 pixels = sum of 55
        // V1 Expected: {30: 55.0}
        // V2 Expected: {30: [1.0, 2.0, 3.0, ..., 10.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            for (int i = 0; i < 10; i++) {
                std::vector<uint32_t> x(i + 1, 1);
                std::vector<uint32_t> y(i + 1, 1);
                mask_data->addAtTime(TimeFrameIndex(30), Mask2D(x, y), NotifyObservers::No);
            }
            storeMaskData("large_mask_count", mask_data);
        }
        
        // =================================================================
        // JSON pipeline tests
        // =================================================================
        
        // Basic JSON pipeline test: two masks at different timestamps
        // V1 Expected: {100: 3.0, 200: 4.0}
        // V2 Expected: {100: [3.0], 200: [4.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            std::vector<uint32_t> x1 = {1, 2, 3};
            std::vector<uint32_t> y1 = {1, 2, 3};
            mask_data->addAtTime(TimeFrameIndex(100), Mask2D(x1, y1), NotifyObservers::No);
            
            std::vector<uint32_t> x2 = {4, 5, 6, 7};
            std::vector<uint32_t> y2 = {4, 5, 6, 7};
            mask_data->addAtTime(TimeFrameIndex(200), Mask2D(x2, y2), NotifyObservers::No);
            
            storeMaskData("json_pipeline_basic", mask_data);
        }
        
        // Multi-timestamp JSON pipeline test
        // V1 Expected: {100: 3.0, 200: 5.0, 300: 2.0}
        // V2 Expected: {100: [3.0], 200: [5.0], 300: [2.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            std::vector<uint32_t> x1 = {1, 2, 3};
            std::vector<uint32_t> y1 = {1, 2, 3};
            mask_data->addAtTime(TimeFrameIndex(100), Mask2D(x1, y1), NotifyObservers::No);
            
            std::vector<uint32_t> x2 = {4, 5, 6, 7, 8};
            std::vector<uint32_t> y2 = {4, 5, 6, 7, 8};
            mask_data->addAtTime(TimeFrameIndex(200), Mask2D(x2, y2), NotifyObservers::No);
            
            std::vector<uint32_t> x3 = {9, 10};
            std::vector<uint32_t> y3 = {9, 10};
            mask_data->addAtTime(TimeFrameIndex(300), Mask2D(x3, y3), NotifyObservers::No);
            
            storeMaskData("json_pipeline_multi_timestamp", mask_data);
        }
        
        // Multiple masks at same timestamp for JSON tests
        // V1 Expected: {500: 5.0} (2 + 3)
        // V2 Expected: {500: [2.0, 3.0]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            std::vector<uint32_t> x1 = {1, 2};
            std::vector<uint32_t> y1 = {1, 2};
            mask_data->addAtTime(TimeFrameIndex(500), Mask2D(x1, y1), NotifyObservers::No);
            
            std::vector<uint32_t> x2 = {3, 4, 5};
            std::vector<uint32_t> y2 = {3, 4, 5};
            mask_data->addAtTime(TimeFrameIndex(500), Mask2D(x2, y2), NotifyObservers::No);
            
            storeMaskData("json_pipeline_multi_mask", mask_data);
        }
    }
    
    void createEmptyMaskData(const std::string& key) {
        auto mask_data = std::make_shared<MaskData>();
        storeMaskData(key, mask_data);
    }
    
    void storeMaskData(const std::string& key, std::shared_ptr<MaskData> mask_data) {
        m_data_manager->setData(key, mask_data, TimeKey("default"));
        m_test_masks[key] = mask_data;
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
};

#endif // MASK_AREA_TEST_FIXTURE_HPP
