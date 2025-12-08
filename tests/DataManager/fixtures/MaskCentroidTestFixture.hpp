#ifndef MASK_CENTROID_TEST_FIXTURE_HPP
#define MASK_CENTROID_TEST_FIXTURE_HPP

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
 * @brief Test fixture for MaskCentroid transform tests
 * 
 * This fixture provides pre-populated MaskData objects for testing
 * the mask centroid calculation transform.
 * 
 * Test data scenarios:
 * - empty_mask_data: No masks (tests empty input handling)
 * - single_mask_triangle: Triangle mask at timestamp 10, centroid at (1,1)
 * - multiple_masks_single_timestamp: Two square masks at timestamp 20
 * - masks_multiple_timestamps: Single masks at timestamps 30 and 40
 * - mask_with_image_size: Mask with image size set (640x480)
 * - empty_mask_at_timestamp: A mask with zero pixels at timestamp 10
 * - mixed_empty_nonempty: Empty and non-empty masks at same timestamp 20
 * - single_point_masks: Two single-point masks at timestamp 30
 * - large_coordinates: Mask with large coordinate values at timestamp 40
 * - json_pipeline_basic: Triangle, square, and multi-mask at timestamps 100, 200, 300
 */
class MaskCentroidTestFixture {
protected:
    MaskCentroidTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    ~MaskCentroidTestFixture() = default;

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
        // Expected: empty PointData (no times with data)
        createEmptyMaskData("empty_mask_data");
        
        // Single mask at single timestamp - triangle (3 points)
        // Vertices at (0,0), (3,0), (0,3) -> centroid at (1,1)
        // Expected: {10: [(1.0, 1.0)]}
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> x = {0, 3, 0};
            std::vector<uint32_t> y = {0, 0, 3};
            mask_data->addAtTime(TimeFrameIndex(10), Mask2D(x, y), NotifyObservers::No);
            storeMaskData("single_mask_triangle", mask_data);
        }
        
        // Multiple masks at single timestamp - two squares
        // First: (0,0), (1,0), (0,1), (1,1) -> centroid at (0.5, 0.5)
        // Second: (4,4), (5,4), (4,5), (5,5) -> centroid at (4.5, 4.5)
        // Expected: {20: [(0.5, 0.5), (4.5, 4.5)]}
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> x1 = {0, 1, 0, 1};
            std::vector<uint32_t> y1 = {0, 0, 1, 1};
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(x1, y1), NotifyObservers::No);
            
            std::vector<uint32_t> x2 = {4, 5, 4, 5};
            std::vector<uint32_t> y2 = {4, 4, 5, 5};
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(x2, y2), NotifyObservers::No);
            storeMaskData("multiple_masks_single_timestamp", mask_data);
        }
        
        // Single masks across multiple timestamps
        // Timestamp 30: Horizontal line (0,0), (2,0), (4,0) -> centroid at (2, 0)
        // Timestamp 40: Vertical line (1,0), (1,3), (1,6) -> centroid at (1, 3)
        // Expected: {30: [(2.0, 0.0)], 40: [(1.0, 3.0)]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            std::vector<uint32_t> x1 = {0, 2, 4};
            std::vector<uint32_t> y1 = {0, 0, 0};
            mask_data->addAtTime(TimeFrameIndex(30), Mask2D(x1, y1), NotifyObservers::No);
            
            std::vector<uint32_t> x2 = {1, 1, 1};
            std::vector<uint32_t> y2 = {0, 3, 6};
            mask_data->addAtTime(TimeFrameIndex(40), Mask2D(x2, y2), NotifyObservers::No);
            
            storeMaskData("masks_multiple_timestamps", mask_data);
        }
        
        // Mask with image size set
        // Image size: 640x480
        // Points: (100, 100), (200, 150), (300, 200) -> centroid at (200, 150)
        // Expected: {100: [(200.0, 150.0)]} with image size preserved
        {
            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize(ImageSize{640, 480});
            
            std::vector<uint32_t> x = {100, 200, 300};
            std::vector<uint32_t> y = {100, 150, 200};
            mask_data->addAtTime(TimeFrameIndex(100), Mask2D(x, y), NotifyObservers::No);
            storeMaskData("mask_with_image_size", mask_data);
        }
        
        // =================================================================
        // Edge cases
        // =================================================================
        
        // Empty mask (mask with zero pixels) at a timestamp
        // Expected: empty PointData (empty masks are skipped)
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> empty_x;
            std::vector<uint32_t> empty_y;
            mask_data->addAtTime(TimeFrameIndex(10), Mask2D(empty_x, empty_y), NotifyObservers::No);
            storeMaskData("empty_mask_at_timestamp", mask_data);
        }
        
        // Mixed empty and non-empty masks at same timestamp
        // Empty mask + mask with points (2,1), (4,3) -> centroid at (3, 2)
        // Expected: {20: [(3.0, 2.0)]} (empty mask skipped)
        {
            auto mask_data = std::make_shared<MaskData>();
            
            std::vector<uint32_t> empty_x;
            std::vector<uint32_t> empty_y;
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(empty_x, empty_y), NotifyObservers::No);
            
            std::vector<uint32_t> x = {2, 4};
            std::vector<uint32_t> y = {1, 3};
            mask_data->addAtTime(TimeFrameIndex(20), Mask2D(x, y), NotifyObservers::No);
            
            storeMaskData("mixed_empty_nonempty", mask_data);
        }
        
        // Single point masks - two masks each with one point
        // First: (5, 7) -> centroid at (5, 7)
        // Second: (10, 15) -> centroid at (10, 15)
        // Expected: {30: [(5.0, 7.0), (10.0, 15.0)]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            std::vector<uint32_t> x1 = {5};
            std::vector<uint32_t> y1 = {7};
            mask_data->addAtTime(TimeFrameIndex(30), Mask2D(x1, y1), NotifyObservers::No);
            
            std::vector<uint32_t> x2 = {10};
            std::vector<uint32_t> y2 = {15};
            mask_data->addAtTime(TimeFrameIndex(30), Mask2D(x2, y2), NotifyObservers::No);
            
            storeMaskData("single_point_masks", mask_data);
        }
        
        // Large coordinate values to test for overflow
        // Points: (1000000, 2000000), (1000001, 2000001), (1000002, 2000002)
        // -> centroid at (1000001, 2000001)
        // Expected: {40: [(1000001.0, 2000001.0)]}
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> x = {1000000, 1000001, 1000002};
            std::vector<uint32_t> y = {2000000, 2000001, 2000002};
            mask_data->addAtTime(TimeFrameIndex(40), Mask2D(x, y), NotifyObservers::No);
            storeMaskData("large_coordinates", mask_data);
        }
        
        // =================================================================
        // JSON pipeline tests
        // =================================================================
        
        // Basic JSON pipeline test: triangle, square, and multi-mask
        // Timestamp 100: Triangle (0,0), (3,0), (0,3) -> centroid at (1, 1)
        // Timestamp 200: Square (1,1), (3,1), (1,3), (3,3) -> centroid at (2, 2)
        // Timestamp 300: Two squares -> centroids at (1, 1) and (6, 6)
        // Expected: {100: [(1,1)], 200: [(2,2)], 300: [(1,1), (6,6)]}
        {
            auto mask_data = std::make_shared<MaskData>();
            
            // Timestamp 100: Triangle
            std::vector<uint32_t> x1 = {0, 3, 0};
            std::vector<uint32_t> y1 = {0, 0, 3};
            mask_data->addAtTime(TimeFrameIndex(100), Mask2D(x1, y1), NotifyObservers::No);
            
            // Timestamp 200: Square
            std::vector<uint32_t> x2 = {1, 3, 1, 3};
            std::vector<uint32_t> y2 = {1, 1, 3, 3};
            mask_data->addAtTime(TimeFrameIndex(200), Mask2D(x2, y2), NotifyObservers::No);
            
            // Timestamp 300: Two squares
            std::vector<uint32_t> x3a = {0, 2, 0, 2};
            std::vector<uint32_t> y3a = {0, 0, 2, 2};
            mask_data->addAtTime(TimeFrameIndex(300), Mask2D(x3a, y3a), NotifyObservers::No);
            
            std::vector<uint32_t> x3b = {5, 7, 5, 7};
            std::vector<uint32_t> y3b = {5, 5, 7, 7};
            mask_data->addAtTime(TimeFrameIndex(300), Mask2D(x3b, y3b), NotifyObservers::No);
            
            storeMaskData("json_pipeline_basic", mask_data);
        }
        
        // Horizontal line mask for operation execute test
        // Points: (0, 0), (2, 0), (4, 0) -> centroid at (2, 0)
        // Expected: {50: [(2.0, 0.0)]}
        {
            auto mask_data = std::make_shared<MaskData>();
            std::vector<uint32_t> x = {0, 2, 4};
            std::vector<uint32_t> y = {0, 0, 0};
            mask_data->addAtTime(TimeFrameIndex(50), Mask2D(x, y), NotifyObservers::No);
            storeMaskData("operation_execute_test", mask_data);
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

#endif // MASK_CENTROID_TEST_FIXTURE_HPP
