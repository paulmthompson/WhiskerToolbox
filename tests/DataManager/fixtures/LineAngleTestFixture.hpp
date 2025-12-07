#ifndef LINE_ANGLE_TEST_FIXTURE_HPP
#define LINE_ANGLE_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <map>
#include <string>
#include <vector>

/**
 * @brief Test fixture for Line Angle transform tests
 * 
 * This fixture provides reusable test data for both V1 and V2 tests.
 * Each test scenario is stored with a descriptive key that describes the
 * data pattern, not the expected result.
 * 
 * The fixture creates LineData objects for testing angle calculations.
 * Expected results are documented in comments but not encoded in the fixture.
 */
class LineAngleTestFixture {
protected:
    LineAngleTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_time_frame = std::make_shared<TimeFrame>();
        m_data_manager->setTime(TimeKey("default"), m_time_frame);
        populateTestData();
    }

    ~LineAngleTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    // Map of named test line data - primary inputs
    std::map<std::string, std::shared_ptr<LineData>> m_line_data;

private:
    void populateTestData() {
        // ========================================================================
        // Core Functionality Test Data
        // ========================================================================

        // Scenario: Horizontal line pointing right
        // Line: (0,1) to (3,1) at t=10
        // Expected angle at position 0.33: 0 degrees (horizontal)
        createLineData("horizontal_line",
            /*timestamp=*/10,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f},
            /*y_coords=*/{1.0f, 1.0f, 1.0f, 1.0f});

        // Scenario: Vertical line pointing up
        // Line: (1,0) to (1,3) at t=20
        // Expected angle at position 0.25: 90 degrees (vertical)
        createLineData("vertical_line",
            /*timestamp=*/20,
            /*x_coords=*/{1.0f, 1.0f, 1.0f, 1.0f},
            /*y_coords=*/{0.0f, 1.0f, 2.0f, 3.0f});

        // Scenario: Diagonal line at 45 degrees
        // Line: (0,0) to (3,3) at t=30
        // Expected angle at position 0.5: 45 degrees
        createLineData("diagonal_45_degrees",
            /*timestamp=*/30,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f},
            /*y_coords=*/{0.0f, 1.0f, 2.0f, 3.0f});

        // Scenario: Multiple lines at different timestamps
        // t=40: horizontal, t=50: vertical, t=60: 45-degree
        createMultiTimestepData("multiple_timesteps");

        // Scenario: Parabolic curve (y = x^2)
        // Line: points on a parabola at t=70
        // Expected: polynomial fit should capture curvature
        createLineData("parabola",
            /*timestamp=*/70,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
            /*y_coords=*/{0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f});

        // Scenario: Smooth curve for polynomial order testing
        // Line: smooth curve at t=80
        createLineData("smooth_curve",
            /*timestamp=*/80,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
            /*y_coords=*/{0.0f, 0.5f, 1.8f, 3.9f, 6.8f, 10.5f, 15.0f, 20.3f});

        // Scenario: Simple horizontal line at origin
        // Line: (0,0) to (3,0) at t=100
        // Expected angle: 0 degrees
        createLineData("horizontal_at_origin",
            /*timestamp=*/100,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f},
            /*y_coords=*/{0.0f, 0.0f, 0.0f, 0.0f});

        // ========================================================================
        // Reference Vector Test Data
        // ========================================================================

        // Scenario: 45-degree line for reference vector tests
        // Line: (0,0) to (3,3) at t=110
        // Used for testing different reference vectors
        createLineData("diagonal_for_reference",
            /*timestamp=*/110,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f},
            /*y_coords=*/{0.0f, 1.0f, 2.0f, 3.0f});

        // Scenario: Horizontal line for 45-degree reference test
        // Line: (0,1) to (3,1) at t=130
        createLineData("horizontal_for_reference",
            /*timestamp=*/130,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f},
            /*y_coords=*/{1.0f, 1.0f, 1.0f, 1.0f});

        // Scenario: Parabolic curve for polynomial reference tests
        // Line: y = x^2 at t=140
        createLineData("parabola_for_reference",
            /*timestamp=*/140,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
            /*y_coords=*/{0.0f, 1.0f, 4.0f, 9.0f, 16.0f});

        // ========================================================================
        // Edge Cases Test Data
        // ========================================================================

        // Scenario: Line with only one point (invalid)
        createLineData("single_point_line",
            /*timestamp=*/10,
            /*x_coords=*/{1.0f},
            /*y_coords=*/{1.0f});

        // Scenario: Two-point diagonal line
        // Line: (0,0) to (3,3) at t=20
        createLineData("two_point_diagonal",
            /*timestamp=*/20,
            /*x_coords=*/{0.0f, 3.0f},
            /*y_coords=*/{0.0f, 3.0f});

        // Scenario: Line with few points for polynomial fallback test
        // Line: (0,0) to (1,1) at t=40
        createLineData("two_point_line",
            /*timestamp=*/40,
            /*x_coords=*/{0.0f, 1.0f},
            /*y_coords=*/{0.0f, 1.0f});

        // Scenario: Vertical collinear line (all x values same)
        // Tests polynomial fit with collinear points
        createLineData("vertical_collinear",
            /*timestamp=*/50,
            /*x_coords=*/{1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            /*y_coords=*/{0.0f, 1.0f, 2.0f, 3.0f, 4.0f});

        // Scenario: Simple 45-degree line for null params test
        createLineData("simple_diagonal",
            /*timestamp=*/60,
            /*x_coords=*/{0.0f, 1.0f, 2.0f},
            /*y_coords=*/{0.0f, 1.0f, 2.0f});

        // Scenario: Large line with 1000 points (stress test)
        createLargeLineData("large_diagonal_line", /*timestamp=*/70, /*num_points=*/1000);

        // Scenario: Horizontal line for reference normalization test
        createLineData("horizontal_for_normalization",
            /*timestamp=*/90,
            /*x_coords=*/{0.0f, 1.0f, 2.0f, 3.0f},
            /*y_coords=*/{0.0f, 0.0f, 0.0f, 0.0f});

        // Scenario: 2-point problematic lines with negative coordinates
        createLineData("problematic_line_1",
            /*timestamp=*/200,
            /*x_coords=*/{565.0f, 408.0f},
            /*y_coords=*/{253.0f, 277.0f});

        createLineData("problematic_line_2",
            /*timestamp=*/210,
            /*x_coords=*/{567.0f, 434.0f},
            /*y_coords=*/{252.0f, 265.0f});

        // ========================================================================
        // JSON Pipeline Test Data
        // ========================================================================

        // Scenario: Basic JSON pipeline test
        // Horizontal line at t=100, diagonal at t=200
        createJsonPipelineTwoTimesteps("json_pipeline_two_timesteps");

        // Scenario: Multiple lines for JSON pipeline test
        createJsonPipelineMultipleAngles("json_pipeline_multiple_angles");

        // ========================================================================
        // Empty Data
        // ========================================================================
        
        // Scenario: Empty line data
        createEmptyLineData("empty_line_data");
    }

    void createLineData(const std::string& key, 
                        int timestamp,
                        const std::vector<float>& x_coords,
                        const std::vector<float>& y_coords) {
        auto line_data = std::make_shared<LineData>();
        line_data->setTimeFrame(m_time_frame);
        
        TimeFrameIndex time_idx(timestamp);
        line_data->emplaceAtTime(time_idx, 
            std::vector<float>(x_coords), 
            std::vector<float>(y_coords));
        
        // Store in map for direct access
        m_line_data[key] = line_data;
        
        // Store in DataManager with distinct key
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
    }

    void createMultiTimestepData(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        line_data->setTimeFrame(m_time_frame);
        
        // Timestamp 40: horizontal line
        line_data->emplaceAtTime(TimeFrameIndex(40), 
            std::vector<float>{0.0f, 1.0f, 2.0f}, 
            std::vector<float>{1.0f, 1.0f, 1.0f});
        
        // Timestamp 50: vertical line
        line_data->emplaceAtTime(TimeFrameIndex(50), 
            std::vector<float>{1.0f, 1.0f, 1.0f}, 
            std::vector<float>{0.0f, 1.0f, 2.0f});
        
        // Timestamp 60: 45-degree line
        line_data->emplaceAtTime(TimeFrameIndex(60), 
            std::vector<float>{0.0f, 1.0f, 2.0f}, 
            std::vector<float>{0.0f, 1.0f, 2.0f});
        
        m_line_data[key] = line_data;
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
    }

    void createLargeLineData(const std::string& key, int timestamp, int num_points) {
        auto line_data = std::make_shared<LineData>();
        line_data->setTimeFrame(m_time_frame);
        
        std::vector<float> x_coords;
        std::vector<float> y_coords;
        for (int i = 0; i < num_points; ++i) {
            x_coords.push_back(static_cast<float>(i));
            y_coords.push_back(static_cast<float>(i));  // 45-degree line
        }
        
        line_data->emplaceAtTime(TimeFrameIndex(timestamp), 
            std::move(x_coords), 
            std::move(y_coords));
        
        m_line_data[key] = line_data;
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
    }

    void createJsonPipelineTwoTimesteps(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        line_data->setTimeFrame(m_time_frame);
        
        // Timestamp 100: horizontal line (0 degrees)
        line_data->emplaceAtTime(TimeFrameIndex(100), 
            std::vector<float>{0.0f, 1.0f, 2.0f, 3.0f}, 
            std::vector<float>{0.0f, 0.0f, 0.0f, 0.0f});
        
        // Timestamp 200: 45-degree line
        line_data->emplaceAtTime(TimeFrameIndex(200), 
            std::vector<float>{0.0f, 1.0f, 2.0f, 3.0f}, 
            std::vector<float>{0.0f, 1.0f, 2.0f, 3.0f});
        
        m_line_data[key] = line_data;
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
    }

    void createJsonPipelineMultipleAngles(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        line_data->setTimeFrame(m_time_frame);
        
        // Timestamp 100: horizontal line (0 degrees)
        line_data->emplaceAtTime(TimeFrameIndex(100), 
            std::vector<float>{0.0f, 1.0f, 2.0f}, 
            std::vector<float>{0.0f, 0.0f, 0.0f});
        
        // Timestamp 200: vertical line (90 degrees)
        line_data->emplaceAtTime(TimeFrameIndex(200), 
            std::vector<float>{0.0f, 0.0f, 0.0f}, 
            std::vector<float>{0.0f, 1.0f, 2.0f});
        
        // Timestamp 300: 45-degree line
        line_data->emplaceAtTime(TimeFrameIndex(300), 
            std::vector<float>{0.0f, 1.0f, 2.0f}, 
            std::vector<float>{0.0f, 1.0f, 2.0f});
        
        m_line_data[key] = line_data;
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
    }

    void createEmptyLineData(const std::string& key) {
        auto line_data = std::make_shared<LineData>();
        line_data->setTimeFrame(m_time_frame);
        
        m_line_data[key] = line_data;
        m_data_manager->setData(key + "_line", line_data, TimeKey("default"));
    }

    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<TimeFrame> m_time_frame;
};

#endif // LINE_ANGLE_TEST_FIXTURE_HPP
