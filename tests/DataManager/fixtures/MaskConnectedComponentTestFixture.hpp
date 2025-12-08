#ifndef MASK_CONNECTED_COMPONENT_TEST_FIXTURE_HPP
#define MASK_CONNECTED_COMPONENT_TEST_FIXTURE_HPP

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
 * @brief Test fixture for MaskConnectedComponent transform tests
 * 
 * This fixture provides pre-populated MaskData objects for testing
 * the mask connected component filtering transform.
 * 
 * Test data scenarios:
 * - empty_mask_data: No masks (tests empty input handling)
 * - large_and_small_components: 3x3 large (9px), 1px small, 2px small at same timestamp
 * - multiple_small_components: Several 1-2 pixel components at same timestamp
 * - medium_components: Two medium-sized components (3px and 2px)
 * - multiple_timestamps: Different components across 3 timestamps
 * - json_pipeline_mixed: Large (9px), small (1px), medium (4px) for JSON tests
 */
class MaskConnectedComponentTestFixture {
protected:
    MaskConnectedComponentTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    ~MaskConnectedComponentTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    std::shared_ptr<TimeFrame> getTimeFrame() {
        return m_time_frame;
    }

    /// Map of named test data objects for direct access
    std::map<std::string, std::shared_ptr<MaskData>> m_test_masks;

private:
    void populateTestData() {
        // =================================================================
        // Core functionality tests
        // =================================================================
        
        // Empty mask data - no masks at all
        createEmptyMaskData("empty_mask_data");
        
        // Large component (9 pixels 3x3 square) + small components (1px, 2px)
        // Used for testing basic filtering
        // With threshold=5: should keep 9px component, remove 1px and 2px
        {
            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize({10, 10});
            mask_data->setTimeFrame(m_time_frame);
            
            // Large component: 3x3 square = 9 pixels
            Mask2D large_component = {
                {1, 1}, {2, 1}, {3, 1},  // Row 1
                {1, 2}, {2, 2}, {3, 2},  // Row 2  
                {1, 3}, {2, 3}, {3, 3}   // Row 3
            };
            
            // Small component: 1 pixel
            Mask2D small_component1 = {
                {7, 1}
            };
            
            // Small component: 2 pixels
            Mask2D small_component2 = {
                {7, 7}, {8, 7}
            };
            
            mask_data->addAtTime(TimeFrameIndex(0), large_component, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(0), small_component1, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(0), small_component2, NotifyObservers::No);
            
            storeMaskData("large_and_small_components", mask_data);
        }
        
        // Multiple small components (all 1-2 pixels)
        // Used for testing threshold=1 (preserve all)
        // Total: 1 + 1 + 2 = 4 pixels
        {
            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize({5, 5});
            mask_data->setTimeFrame(m_time_frame);
            
            Mask2D component1 = {{1, 1}};
            Mask2D component2 = {{3, 3}};
            Mask2D component3 = {{0, 4}, {1, 4}};  // 2-pixel component
            
            mask_data->addAtTime(TimeFrameIndex(10), component1, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(10), component2, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(10), component3, NotifyObservers::No);
            
            storeMaskData("multiple_small_components", mask_data);
        }
        
        // Medium-sized components (3px and 2px)
        // Used for testing high threshold (remove all)
        // With threshold=10: should remove all
        {
            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize({10, 10});
            mask_data->setTimeFrame(m_time_frame);
            
            Mask2D component1 = {{0, 0}, {1, 0}, {0, 1}};  // 3 pixels
            Mask2D component2 = {{5, 5}, {6, 5}};  // 2 pixels
            
            mask_data->addAtTime(TimeFrameIndex(5), component1, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(5), component2, NotifyObservers::No);
            
            storeMaskData("medium_components", mask_data);
        }
        
        // Multiple timestamps with different component sizes
        // Time 0: 6 pixels (should be preserved with threshold=4)
        // Time 1: 2 pixels (should be removed with threshold=4)
        // Time 2: 5 pixels (should be preserved with threshold=4)
        {
            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize({8, 8});
            mask_data->setTimeFrame(m_time_frame);
            
            // Time 0: Large component (6 pixels)
            Mask2D large_comp = {
                {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}
            };
            
            // Time 1: Small component (2 pixels)
            Mask2D small_comp = {
                {5, 5}, {5, 6}
            };
            
            // Time 2: Medium component (5 pixels)
            Mask2D medium_comp = {
                {3, 3}, {4, 3}, {3, 4}, {4, 4}, {3, 5}
            };
            
            mask_data->addAtTime(TimeFrameIndex(0), large_comp, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(1), small_comp, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(2), medium_comp, NotifyObservers::No);
            
            storeMaskData("multiple_timestamps", mask_data);
        }
        
        // =================================================================
        // Operation interface tests
        // =================================================================
        
        // Large component (12 pixels) + small component (1 pixel)
        // Used for testing default threshold (10)
        // With threshold=10: should keep 12px, remove 1px
        {
            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize({6, 6});
            mask_data->setTimeFrame(m_time_frame);
            
            Mask2D large_comp = {
                {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1},
                {0, 2}, {1, 2}, {2, 2}, {3, 0}, {3, 1}, {3, 2}  // 12 pixels
            };
            
            Mask2D small_comp = {
                {5, 5}  // 1 pixel
            };
            
            mask_data->addAtTime(TimeFrameIndex(0), large_comp, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(0), small_comp, NotifyObservers::No);
            
            storeMaskData("operation_test_data", mask_data);
        }
        
        // =================================================================
        // JSON pipeline tests
        // =================================================================
        
        // Mixed components for JSON pipeline testing
        // Large (9px), small (1px), medium (4px)
        // With threshold=3: keep large + medium (13 total)
        // With threshold=5: keep large only (9 total)
        // With threshold=1: keep all (14 total)
        {
            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize({10, 10});
            mask_data->setTimeFrame(m_time_frame);
            
            // Large component (9 pixels) - 3x3 square
            Mask2D large_component = {
                {1, 1}, {2, 1}, {3, 1},
                {1, 2}, {2, 2}, {3, 2},
                {1, 3}, {2, 3}, {3, 3}
            };
            
            // Small component (1 pixel)
            Mask2D small_component = {
                {7, 1}
            };
            
            // Medium component (4 pixels) - 2x2 square
            Mask2D medium_component = {
                {5, 5}, {6, 5}, {5, 6}, {6, 6}
            };
            
            mask_data->addAtTime(TimeFrameIndex(0), large_component, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(0), small_component, NotifyObservers::No);
            mask_data->addAtTime(TimeFrameIndex(0), medium_component, NotifyObservers::No);
            
            storeMaskData("json_pipeline_mixed", mask_data);
        }
    }
    
    void createEmptyMaskData(const std::string& key) {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setTimeFrame(m_time_frame);
        storeMaskData(key, mask_data);
    }
    
    void storeMaskData(const std::string& key, std::shared_ptr<MaskData> mask_data) {
        m_data_manager->setData(key, mask_data, TimeKey("default"));
        m_test_masks[key] = mask_data;
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
};

#endif // MASK_CONNECTED_COMPONENT_TEST_FIXTURE_HPP
