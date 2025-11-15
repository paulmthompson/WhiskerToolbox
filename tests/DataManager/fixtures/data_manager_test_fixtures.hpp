#ifndef DATA_MANAGER_TEST_FIXTURES_HPP
#define DATA_MANAGER_TEST_FIXTURES_HPP

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "DataManager.hpp"
#include "Points/Point_Data.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <map>
#include <random>

/**
 * @brief Test fixture for DataManager with comprehensive test data
 * 
 * This fixture provides a DataManager populated with various types of test data
 * including PointData, LineData, MaskData, AnalogTimeSeries, DigitalEventSeries,
 * and DigitalIntervalSeries. It's designed for testing DataManager functionality
 * and data visualization components.
 */
class DataManagerTestFixture {
protected:
    DataManagerTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();
        
        // Populate with test data
        populateWithTestData();
    }
    
    ~DataManagerTestFixture() = default;
    
    /**
     * @brief Get the DataManager instance
     * @return Reference to the DataManager
     */
    DataManager& getDataManager() { return *m_data_manager; }
    
    /**
     * @brief Get the DataManager instance (const version)
     * @return Const reference to the DataManager
     */
    const DataManager& getDataManager() const { return *m_data_manager; }
    
    /**
     * @brief Get a pointer to the DataManager
     * @return Raw pointer to the DataManager
     */
    DataManager* getDataManagerPtr() { return m_data_manager.get(); }

private:
    std::unique_ptr<DataManager> m_data_manager;
    
    /**
     * @brief Populate the DataManager with comprehensive test data
     */
    void populateWithTestData() {
        createTestPointData();
        createTestLineData();
        createTestMaskData();
        createTestAnalogTimeSeries();
        createTestDigitalEventSeries();
        createTestDigitalIntervalSeries();
    }
    
    /**
     * @brief Create and add test PointData to the DataManager
     */
    void createTestPointData() {
        auto point_data = std::make_shared<PointData>();
        
        // Create test points at different time frames
        std::vector<Point2D<float>> points_frame_1 = {
            {10.0f, 20.0f},
            {15.0f, 25.0f},
            {20.0f, 30.0f},
            {25.0f, 35.0f}
        };
        
        std::vector<Point2D<float>> points_frame_2 = {
            {30.0f, 40.0f},
            {35.0f, 45.0f},
            {40.0f, 50.0f}
        };
        
        std::vector<Point2D<float>> points_frame_3 = {
            {50.0f, 60.0f},
            {55.0f, 65.0f},
            {60.0f, 70.0f},
            {65.0f, 75.0f},
            {70.0f, 80.0f}
        };
        
        // Add points to different time frames
        point_data->addAtTime(TimeFrameIndex(1), points_frame_1, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(2), points_frame_2, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(3), points_frame_3, NotifyObservers::No);
        
        // Set image size for the point data
        point_data->setImageSize(ImageSize(800, 600));
        
        // Add to DataManager
        m_data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));
    }
    
    /**
     * @brief Create and add test LineData to the DataManager
     */
    void createTestLineData() {
        auto line_data = std::make_shared<LineData>();
        
        // Create test lines at different time frames
        std::vector<Point2D<float>> line1_frame_1 = {
            {100.0f, 150.0f},
            {120.0f, 160.0f},
            {140.0f, 170.0f},
            {160.0f, 180.0f}
        };
        
        std::vector<Point2D<float>> line2_frame_1 = {
            {200.0f, 250.0f},
            {220.0f, 260.0f},
            {240.0f, 270.0f}
        };
        
        std::vector<Point2D<float>> line1_frame_2 = {
            {300.0f, 350.0f},
            {320.0f, 360.0f},
            {340.0f, 370.0f},
            {360.0f, 380.0f},
            {380.0f, 390.0f}
        };
        
        // Create Line2D objects
        Line2D line1_1(line1_frame_1);
        Line2D line2_1(line2_frame_1);
        Line2D line1_2(line1_frame_2);
        
        // Add lines to different time frames
        line_data->addAtTime(TimeFrameIndex(1), line1_1, NotifyObservers::No);
        line_data->addAtTime(TimeFrameIndex(1), line2_1, NotifyObservers::No);
        line_data->addAtTime(TimeFrameIndex(2), line1_2, NotifyObservers::No);
        
        // Set image size for the line data
        line_data->setImageSize(ImageSize(800, 600));
        
        // Add to DataManager
        m_data_manager->setData<LineData>("test_lines", line_data, TimeKey("time"));
    }
    
    /**
     * @brief Create and add test MaskData to the DataManager
     */
    void createTestMaskData() {
        auto mask_data = std::make_shared<MaskData>();
        
        // Create test masks at different time frames
        Mask2D mask1_frame_1 = {
            {100, 100},
            {101, 100},
            {102, 100},
            {100, 101},
            {101, 101},
            {102, 101},
            {100, 102},
            {101, 102},
            {102, 102}
        };
        
        Mask2D mask2_frame_1 = {
            {200, 200},
            {201, 200},
            {202, 200},
            {200, 201},
            {201, 201},
            {202, 201}
        };
        
        Mask2D mask1_frame_2 = {
            {300, 300},
            {301, 300},
            {302, 300},
            {303, 300},
            {300, 301},
            {301, 301},
            {302, 301},
            {303, 301},
            {300, 302},
            {301, 302},
            {302, 302},
            {303, 302}
        };
        
        // Add masks to different time frames
        mask_data->addAtTime(TimeFrameIndex(1), mask1_frame_1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(1), mask2_frame_1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(2), mask1_frame_2, NotifyObservers::No);

        // Set image size for the mask data
        mask_data->setImageSize(ImageSize(800, 600));
        
        // Add to DataManager
        m_data_manager->setData<MaskData>("test_masks", mask_data, TimeKey("time"));
    }
    
    /**
     * @brief Create and add test AnalogTimeSeries to the DataManager
     */
    void createTestAnalogTimeSeries() {
        // Create analog data with regular sampling
        std::vector<float> analog_values = {
            0.1f, 0.2f, 0.15f, 0.3f, 0.25f, 0.4f, 0.35f, 0.5f, 0.45f, 0.6f,
            0.55f, 0.7f, 0.65f, 0.8f, 0.75f, 0.9f, 0.85f, 1.0f, 0.95f, 0.8f
        };
        
        // Create time indices (regular sampling)
        std::vector<TimeFrameIndex> time_indices;
        for (size_t i = 0; i < analog_values.size(); ++i) {
            time_indices.emplace_back(static_cast<int64_t>(i));
        }
        
        // Create AnalogTimeSeries
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_values, time_indices);
        
        // Add to DataManager
        m_data_manager->setData<AnalogTimeSeries>("test_analog", analog_series, TimeKey("time"));
        
        // Create a second analog series with different characteristics
        std::vector<float> analog_values_2 = {
            1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.0f, 0.2f, 0.4f, 0.6f, 0.8f,
            1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.0f, 0.2f, 0.4f, 0.6f, 0.8f
        };
        
        std::vector<TimeFrameIndex> time_indices_2;
        for (size_t i = 0; i < analog_values_2.size(); ++i) {
            time_indices_2.emplace_back(static_cast<int64_t>(i * 2)); // Different sampling rate
        }
        
        auto analog_series_2 = std::make_shared<AnalogTimeSeries>(analog_values_2, time_indices_2);
        m_data_manager->setData<AnalogTimeSeries>("test_analog_2", analog_series_2, TimeKey("time"));
    }
    
    /**
     * @brief Create and add test DigitalEventSeries to the DataManager
     */
    void createTestDigitalEventSeries() {
        // Create event times
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(1), TimeFrameIndex(3), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(9), 
            TimeFrameIndex(11), TimeFrameIndex(13), TimeFrameIndex(15), TimeFrameIndex(17), TimeFrameIndex(19)
        };
        
        // Create DigitalEventSeries
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        
        // Add to DataManager
        m_data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));
        
        // Create a second event series with different timing
        std::vector<TimeFrameIndex> event_times_2 = {
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4), 
            TimeFrameIndex(5), TimeFrameIndex(6), TimeFrameIndex(7), TimeFrameIndex(8), TimeFrameIndex(9)
        };
        
        auto event_series_2 = std::make_shared<DigitalEventSeries>(event_times_2);
        m_data_manager->setData<DigitalEventSeries>("test_events_2", event_series_2, TimeKey("time"));
    }
    
    /**
     * @brief Create and add test DigitalIntervalSeries to the DataManager
     */
    void createTestDigitalIntervalSeries() {
        // Create intervals
        std::vector<Interval> intervals = {
            {1, 3},
            {5, 7},
            {9, 11},
            {13, 15},
            {17, 19}
        };
        
        // Create DigitalIntervalSeries
        auto interval_series = std::make_shared<DigitalIntervalSeries>(intervals);
        
        // Add to DataManager
        m_data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));
        
        // Create a second interval series with overlapping intervals
        std::vector<Interval> intervals_2 = {
            {0, 2},
            {2, 4},
            {4, 6},
            {6, 8},
            {8, 10}
        };
        
        auto interval_series_2 = std::make_shared<DigitalIntervalSeries>(intervals_2);
        m_data_manager->setData<DigitalIntervalSeries>("test_intervals_2", interval_series_2, TimeKey("time"));
    }
};

/**
 * @brief Test fixture for DataManager with random test data
 * 
 * This fixture creates DataManager instances with randomly generated test data,
 * useful for stress testing and edge case discovery.
 */
class DataManagerRandomTestFixture {
protected:
    DataManagerRandomTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        m_random_engine = std::make_unique<std::mt19937>(42); // Fixed seed for reproducibility
        populateWithRandomData();
    }
    
    ~DataManagerRandomTestFixture() = default;
    
    /**
     * @brief Get the DataManager instance
     * @return Reference to the DataManager
     */
    DataManager& getDataManager() { return *m_data_manager; }
    
    /**
     * @brief Get the random engine for generating additional random data
     * @return Reference to the random engine
     */
    std::mt19937& getRandomEngine() { return *m_random_engine; }

private:
    std::unique_ptr<DataManager> m_data_manager;
    std::unique_ptr<std::mt19937> m_random_engine;
    
    /**
     * @brief Populate the DataManager with randomly generated test data
     */
    void populateWithRandomData() {
        createRandomPointData();
        createRandomLineData();
        createRandomAnalogTimeSeries();
        createRandomDigitalEventSeries();
        createRandomDigitalIntervalSeries();
    }
    
    /**
     * @brief Create random PointData
     */
    void createRandomPointData() {
        auto point_data = std::make_shared<PointData>();
        std::uniform_real_distribution<float> coord_dist(0.0f, 800.0f);
        std::uniform_int_distribution<int> count_dist(1, 10);
        
        for (int frame = 0; frame < 5; ++frame) {
            std::vector<Point2D<float>> points;
            int num_points = count_dist(*m_random_engine);
            
            for (int i = 0; i < num_points; ++i) {
                points.emplace_back(coord_dist(*m_random_engine), coord_dist(*m_random_engine));
            }
            
            point_data->addAtTime(TimeFrameIndex(frame), points, NotifyObservers::No);
        }
        
        point_data->setImageSize(ImageSize(800, 600));
        m_data_manager->setData<PointData>("random_points", point_data, TimeKey("time"));
    }
    
    /**
     * @brief Create random LineData
     */
    void createRandomLineData() {
        auto line_data = std::make_shared<LineData>();
        std::uniform_real_distribution<float> coord_dist(0.0f, 800.0f);
        std::uniform_int_distribution<int> point_count_dist(3, 8);
        
        for (int frame = 0; frame < 3; ++frame) {
            std::vector<Point2D<float>> line_points;
            int num_points = point_count_dist(*m_random_engine);
            
            for (int i = 0; i < num_points; ++i) {
                line_points.emplace_back(coord_dist(*m_random_engine), coord_dist(*m_random_engine));
            }
            
            Line2D line(line_points);
            line_data->addAtTime(TimeFrameIndex(frame), line, NotifyObservers::No);
        }
        
        line_data->setImageSize(ImageSize(800, 600));
        m_data_manager->setData<LineData>("random_lines", line_data, TimeKey("time"));
    }
    
    /**
     * @brief Create random AnalogTimeSeries
     */
    void createRandomAnalogTimeSeries() {
        std::uniform_real_distribution<float> value_dist(-1.0f, 1.0f);
        std::vector<float> analog_values;
        std::vector<TimeFrameIndex> time_indices;
        
        for (int i = 0; i < 50; ++i) {
            analog_values.push_back(value_dist(*m_random_engine));
            time_indices.emplace_back(i);
        }
        
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_values, time_indices);
        m_data_manager->setData<AnalogTimeSeries>("random_analog", analog_series, TimeKey("time"));
    }
    
    /**
     * @brief Create random DigitalEventSeries
     */
    void createRandomDigitalEventSeries() {
        std::uniform_real_distribution<float> time_dist(0.0f, 100.0f);
        std::vector<TimeFrameIndex> event_times;
        
        for (int i = 0; i < 20; ++i) {
            event_times.push_back(TimeFrameIndex(static_cast<int64_t>(time_dist(*m_random_engine))));
        }
        
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        m_data_manager->setData<DigitalEventSeries>("random_events", event_series, TimeKey("time"));
    }
    
    /**
     * @brief Create random DigitalIntervalSeries
     */
    void createRandomDigitalIntervalSeries() {
        std::uniform_real_distribution<float> time_dist(0.0f, 100.0f);
        std::vector<Interval> intervals;
        
        for (int i = 0; i < 10; ++i) {
            float start = time_dist(*m_random_engine);
            float end = start + time_dist(*m_random_engine) + 1.0f; // Ensure end > start
            intervals.emplace_back(start, end);
        }
        
        auto interval_series = std::make_shared<DigitalIntervalSeries>(intervals);
        m_data_manager->setData<DigitalIntervalSeries>("random_intervals", interval_series, TimeKey("time"));
    }
};

#endif // DATA_MANAGER_TEST_FIXTURES_HPP 