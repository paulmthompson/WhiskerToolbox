#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogSliceGathererComputer.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IAnalogSource.h"
#include "TimeFrame/TimeFrame.hpp"

// Additional includes for extended testing
#include "DataManager.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <memory>
#include <vector>
#include <cstdint>
#include <numeric>
#include <cmath>
#include <nlohmann/json.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Base test fixture for AnalogSliceGathererComputer with realistic analog data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Analog signals: triangular wave, sine wave
 * - Row intervals representing behavior periods  
 * - Cross-timeframe data for testing timeframe conversion
 */
class AnalogSliceGathererTestFixture {
protected:
    AnalogSliceGathererTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();
        
        // Populate with test data
        populateWithAnalogTestData();
    }

    ~AnalogSliceGathererTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

private:
    std::unique_ptr<DataManager> m_data_manager;

    /**
     * @brief Populate the DataManager with analog test data
     */
    void populateWithAnalogTestData() {
        createTimeFrames();
        createAnalogSignals();
        createBehaviorIntervals();
    }

    /**
     * @brief Create TimeFrame objects for different data streams
     */
    void createTimeFrames() {
        // Create "analog_time" timeframe: 0 to 100 (101 points) - analog signal at 10Hz
        std::vector<int> analog_time_values(101);
        std::iota(analog_time_values.begin(), analog_time_values.end(), 0);
        auto analog_time_frame = std::make_shared<TimeFrame>(analog_time_values);
        m_data_manager->setTime(TimeKey("analog_time"), analog_time_frame, true);

        // Create "behavior_time" timeframe: 0, 2, 4, 6, ..., 100 (51 points) - behavior tracking at 5Hz
        std::vector<int> behavior_time_values;
        behavior_time_values.reserve(51);
        for (int i = 0; i <= 50; ++i) {
            behavior_time_values.push_back(i * 2);
        }
        auto behavior_time_frame = std::make_shared<TimeFrame>(behavior_time_values);
        m_data_manager->setTime(TimeKey("behavior_time"), behavior_time_frame, true);
    }

    /**
     * @brief Create analog signals for testing
     */
    void createAnalogSignals() {
        // Create triangular wave signal: 0->50->0 over 101 points
        std::vector<float> triangular_values;
        std::vector<TimeFrameIndex> triangular_times;
        triangular_values.reserve(101);
        triangular_times.reserve(101);

        for (int i = 0; i <= 100; ++i) {
            float value;
            if (i <= 50) {
                // Rising edge: 0 to 50
                value = static_cast<float>(i);
            } else {
                // Falling edge: 50 to 0
                value = static_cast<float>(100 - i);
            }
            triangular_values.push_back(value);
            triangular_times.emplace_back(i);
        }

        auto triangular_signal = std::make_shared<AnalogTimeSeries>(triangular_values, triangular_times);
        m_data_manager->setData<AnalogTimeSeries>("TriangularWave", triangular_signal, TimeKey("analog_time"));

        // Create sine wave signal: amplitude 25, frequency 0.1 
        std::vector<float> sine_values;
        std::vector<TimeFrameIndex> sine_times;
        sine_values.reserve(101);
        sine_times.reserve(101);

        float const frequency = 0.1f;
        float const amplitude = 25.0f;

        for (int i = 0; i <= 100; ++i) {
            float value = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * frequency * static_cast<float>(i));
            sine_values.push_back(value);
            sine_times.emplace_back(i);
        }

        auto sine_signal = std::make_shared<AnalogTimeSeries>(sine_values, sine_times);
        m_data_manager->setData<AnalogTimeSeries>("SineWave", sine_signal, TimeKey("analog_time"));
    }

    /**
     * @brief Create behavior intervals (row intervals for testing)
     */
    void createBehaviorIntervals() {
        // Create behavior periods for analog data slicing
        auto behavior_intervals = std::make_shared<DigitalIntervalSeries>();
        
        // Exploration period 1: time 10-30 (covers triangular wave rising edge)
        behavior_intervals->addEvent(TimeFrameIndex(5), TimeFrameIndex(15));  // behavior_time indices 5-15 = analog time 10-30
        
        // Rest period: time 40-60 (covers triangular wave peak and start of fall)  
        behavior_intervals->addEvent(TimeFrameIndex(20), TimeFrameIndex(30)); // behavior_time indices 20-30 = analog time 40-60
        
        // Exploration period 2: time 70-90 (covers triangular wave falling edge)
        behavior_intervals->addEvent(TimeFrameIndex(35), TimeFrameIndex(45)); // behavior_time indices 35-45 = analog time 70-90

        m_data_manager->setData<DigitalIntervalSeries>("BehaviorPeriods", behavior_intervals, TimeKey("behavior_time"));
    }
};

/**
 * @brief Test fixture combining AnalogSliceGathererTestFixture with TableRegistry and TablePipeline
 * 
 * This fixture provides everything needed to test JSON-based table pipeline execution:
 * - DataManager with analog test data (from AnalogSliceGathererTestFixture)
 * - TableRegistry for managing table configurations
 * - TablePipeline for executing JSON configurations
 */
class AnalogSliceTableRegistryTestFixture : public AnalogSliceGathererTestFixture {
protected:
    AnalogSliceTableRegistryTestFixture()
        : AnalogSliceGathererTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~AnalogSliceTableRegistryTestFixture() = default;

    /**
     * @brief Get the TableRegistry instance
     * @return Reference to the TableRegistry
     */
    TableRegistry & getTableRegistry() { return *m_table_registry_ptr; }

    /**
     * @brief Get the TableRegistry instance (const version)
     * @return Const reference to the TableRegistry
     */
    TableRegistry const & getTableRegistry() const { return *m_table_registry_ptr; }

    /**
     * @brief Get a pointer to the TableRegistry
     * @return Raw pointer to the TableRegistry
     */
    TableRegistry * getTableRegistryPtr() { return m_table_registry_ptr; }

    /**
     * @brief Get the TablePipeline instance
     * @return Reference to the TablePipeline
     */
    TablePipeline & getTablePipeline() { return *m_table_pipeline; }

    /**
     * @brief Get the TablePipeline instance (const version)
     * @return Const reference to the TablePipeline
     */
    TablePipeline const & getTablePipeline() const { return *m_table_pipeline; }

    /**
     * @brief Get a pointer to the TablePipeline
     * @return Raw pointer to the TablePipeline
     */
    TablePipeline * getTablePipelinePtr() { return m_table_pipeline.get(); }

    /**
     * @brief Get the DataManagerExtension instance
     */
    std::shared_ptr<DataManagerExtension> getDataManagerExtension() { 
        if (!m_data_manager_extension) {
            m_data_manager_extension = std::make_shared<DataManagerExtension>(getDataManager());
        }
        return m_data_manager_extension; 
    }

private:
    TableRegistry * m_table_registry_ptr; // Points to DataManager's TableRegistry
    std::unique_ptr<TablePipeline> m_table_pipeline;
    std::shared_ptr<DataManagerExtension> m_data_manager_extension; // Lazy-initialized
};

// Mock implementation of IAnalogSource for testing
class MockAnalogSource : public IAnalogSource {
public:
    MockAnalogSource(std::string name, 
                     std::shared_ptr<TimeFrame> timeFrame,
                     std::vector<float> data)
        : m_name(std::move(name)), 
          m_timeFrame(std::move(timeFrame)), 
          m_data(std::move(data)) {}

    [[nodiscard]] auto getName() const -> std::string const & override {
        return m_name;
    }

    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override {
        return m_timeFrame;
    }

    [[nodiscard]] auto size() const -> size_t override {
        return m_data.size();
    }

    auto getDataInRange(TimeFrameIndex start, TimeFrameIndex end, 
                       TimeFrame const * target_timeFrame) -> std::vector<float> override {
        // Convert TimeFrameIndex to time values for comparison
        auto startTime = target_timeFrame->getTimeAtIndex(start);
        auto endTime = target_timeFrame->getTimeAtIndex(end);
        
        // Find corresponding indices in our timeframe
        auto startIndex = m_timeFrame->getIndexAtTime(startTime);
        auto endIndex = m_timeFrame->getIndexAtTime(endTime);
        
        // Clamp to valid range
        size_t startIdx = std::max(0L, static_cast<long>(startIndex.getValue()));
        size_t endIdx = std::min(static_cast<long>(m_data.size() - 1), static_cast<long>(endIndex.getValue()));
        
        if (startIdx > endIdx || startIdx >= m_data.size()) {
            return std::vector<float>();
        }
        
        return std::vector<float>(m_data.data() + startIdx, m_data.data() + endIdx + 1);
    }

private:
    std::string m_name;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::vector<float> m_data;
};

TEST_CASE("DM - TV - AnalogSliceGathererComputer Basic Functionality", "[AnalogSliceGathererComputer]") {
    
    SECTION("Basic slice gathering with double template") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create analog data: simple linear ramp 0, 1, 2, ..., 9
        std::vector<float> analogData = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestAnalog", timeFrame, analogData);
        
        // Create row intervals for slicing
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)), // Slice: [1, 2, 3]
            TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(7)), // Slice: [5, 6, 7]
            TimeFrameInterval(TimeFrameIndex(8), TimeFrameIndex(9))  // Slice: [8, 9]
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        // Create the computer
        AnalogSliceGathererComputer<std::vector<double>> computer(analogSource);
        
        // Compute the results
        auto [results, entity_ids] = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 3);
        
        // First slice: [1, 2, 3]
        REQUIRE(results[0].size() == 3);
        REQUIRE(results[0][0] == Catch::Approx(1.0));
        REQUIRE(results[0][1] == Catch::Approx(2.0));
        REQUIRE(results[0][2] == Catch::Approx(3.0));
        
        // Second slice: [5, 6, 7]
        REQUIRE(results[1].size() == 3);
        REQUIRE(results[1][0] == Catch::Approx(5.0));
        REQUIRE(results[1][1] == Catch::Approx(6.0));
        REQUIRE(results[1][2] == Catch::Approx(7.0));
        
        // Third slice: [8, 9]
        REQUIRE(results[2].size() == 2);
        REQUIRE(results[2][0] == Catch::Approx(8.0));
        REQUIRE(results[2][1] == Catch::Approx(9.0));
    }
    
    SECTION("Basic slice gathering with float template") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create analog data: sine wave values
        std::vector<float> analogData;
        for (int i = 0; i < 6; ++i) {
            analogData.push_back(std::sin(static_cast<float>(i) * 0.5f));
        }
        auto analogSource = std::make_shared<MockAnalogSource>("SineWave", timeFrame, analogData);
        
        // Create row intervals
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)), // First 3 points
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5))  // Last 3 points
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        // Create the computer with float template
        AnalogSliceGathererComputer<std::vector<float>> computer(analogSource);
        
        // Compute the results
        auto [results, entity_ids] = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 2);
        REQUIRE(results[0].size() == 3);
        REQUIRE(results[1].size() == 3);
        
        // Check that values are approximately correct (sine wave values)
        for (size_t i = 0; i < results[0].size(); ++i) {
            REQUIRE(results[0][i] == Catch::Approx(std::sin(static_cast<float>(i) * 0.5f)));
        }
        for (size_t i = 0; i < results[1].size(); ++i) {
            REQUIRE(results[1][i] == Catch::Approx(std::sin(static_cast<float>(i + 3) * 0.5f)));
        }
    }
    
    SECTION("Single point intervals") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        // Create analog data
        std::vector<float> analogData = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestAnalog", timeFrame, analogData);
        
        // Create single-point intervals
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1)), // Single point: [1]
            TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(3)), // Single point: [3]
            TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(5))  // Single point: [5]
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        AnalogSliceGathererComputer<std::vector<double>> computer(analogSource);
        
        auto [results, entity_ids] = computer.compute(plan);
        
        // Verify results
        REQUIRE(results.size() == 3);
        
        REQUIRE(results[0].size() == 1);
        REQUIRE(results[0][0] == Catch::Approx(1.0));
        
        REQUIRE(results[1].size() == 1);
        REQUIRE(results[1][0] == Catch::Approx(3.0));
        
        REQUIRE(results[2].size() == 1);
        REQUIRE(results[2][0] == Catch::Approx(5.0));
    }

}

TEST_CASE("DM - TV - AnalogSliceGathererComputer Error Handling", "[AnalogSliceGathererComputer][Error]") {
    
    SECTION("ExecutionPlan without intervals throws exception") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        std::vector<float> analogData = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestAnalog", timeFrame, analogData);
        
        // Create execution plan with indices instead of intervals
        std::vector<TimeFrameIndex> indices = {TimeFrameIndex(0), TimeFrameIndex(1)};
        ExecutionPlan plan(indices, timeFrame);
        
        // Create the computer
        AnalogSliceGathererComputer<std::vector<double>> computer(analogSource);
        
        // Should throw an exception
        REQUIRE_THROWS_AS(computer.compute(plan), std::invalid_argument);
    }
}

TEST_CASE("DM - TV - AnalogSliceGathererComputer Template Types", "[AnalogSliceGathererComputer][Templates]") {
    
    SECTION("Test with different numeric types") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2, 3, 4};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        std::vector<float> analogData = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestAnalog", timeFrame, analogData);
        
        std::vector<TimeFrameInterval> rowIntervals = {
            TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3))
        };
        
        ExecutionPlan plan(rowIntervals, timeFrame);
        
        // Test with double
        AnalogSliceGathererComputer<std::vector<double>> doubleComputer(analogSource);
        auto [doubleResults, doubleEntity_ids] = doubleComputer.compute(plan);
        
        REQUIRE(doubleResults.size() == 1);
        REQUIRE(doubleResults[0].size() == 3);
        REQUIRE(doubleResults[0][0] == Catch::Approx(2.5));
        REQUIRE(doubleResults[0][1] == Catch::Approx(3.5));
        REQUIRE(doubleResults[0][2] == Catch::Approx(4.5));
        
        // Test with float
        AnalogSliceGathererComputer<std::vector<float>> floatComputer(analogSource);
        auto [floatResults, floatEntity_ids] = floatComputer.compute(plan);
        
        REQUIRE(floatResults.size() == 1);
        REQUIRE(floatResults[0].size() == 3);
        REQUIRE(floatResults[0][0] == Catch::Approx(2.5f));
        REQUIRE(floatResults[0][1] == Catch::Approx(3.5f));
        REQUIRE(floatResults[0][2] == Catch::Approx(4.5f));
    }
}

TEST_CASE("DM - TV - AnalogSliceGathererComputer Dependency Tracking", "[AnalogSliceGathererComputer][Dependencies]") {
    
    SECTION("getSourceDependency returns correct source name") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSource", timeFrame, analogData);
        
        // Test default source name
        AnalogSliceGathererComputer<std::vector<double>> computer1(analogSource);
        REQUIRE(computer1.getSourceDependency() == "TestSource");
        
        // Test custom source name
        AnalogSliceGathererComputer<std::vector<double>> computer2(analogSource, "CustomSourceName");
        REQUIRE(computer2.getSourceDependency() == "CustomSourceName");
    }
}

TEST_CASE_METHOD(AnalogSliceGathererTestFixture, "DM - TV - AnalogSliceGathererComputer with DataManager fixture", "[AnalogSliceGathererComputer][DataManager][Fixture]") {
    
    SECTION("Test with triangular wave and behavior intervals from fixture") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        
        // Get the analog source from the DataManager
        auto triangular_source = dme->getAnalogSource("TriangularWave");
        
        REQUIRE(triangular_source != nullptr);
        
        // Create row selector from behavior intervals
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        auto behavior_interval_source = dm.getData<DigitalIntervalSeries>("BehaviorPeriods");
        
        REQUIRE(behavior_interval_source != nullptr);
        
        auto behavior_intervals = behavior_interval_source->getIntervalsInRange(
            TimeFrameIndex(0), TimeFrameIndex(50), *behavior_time_frame);
        
        // Convert to TimeFrameIntervals for row selector
        std::vector<TimeFrameInterval> row_intervals;
        for (const auto& interval : behavior_intervals) {
            row_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }

        REQUIRE(row_intervals.size() == 3);
        
        auto row_selector = std::make_unique<IntervalSelector>(row_intervals, behavior_time_frame);
        
        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Add AnalogSliceGathererComputer columns
        builder.addColumn<std::vector<double>>("TriangularSlices_Double", 
            std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(triangular_source, "TriangularWave"));
        
        // Build the table
        TableView table = builder.build();
        
        // Verify table structure
        REQUIRE(table.getRowCount() == 3);
        REQUIRE(table.getColumnCount() == 1);
        REQUIRE(table.hasColumn("TriangularSlices_Double"));
        
        // Get the column data
        auto double_slices = table.getColumnValues<std::vector<double>>("TriangularSlices_Double");
        
        REQUIRE(double_slices.size() == 3);
        
        // Verify the slices contain reasonable data
        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(double_slices[i].size() > 0);
            
            // Values should be in reasonable range (triangular wave goes 0->50->0)
            for (auto value : double_slices[i]) {
                REQUIRE(value >= 0.0);
                REQUIRE(value <= 50.0);
            }
        }
    }
    
    SECTION("Test cross-timeframe analog slice gathering") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        
        // Get sources from different timeframes
        auto triangular_source = dme->getAnalogSource("TriangularWave");  // analog_time frame (101 points)
        auto behavior_interval_source = dm.getData<DigitalIntervalSeries>("BehaviorPeriods"); // behavior_time frame (51 points)
        
        REQUIRE(triangular_source != nullptr);
        REQUIRE(behavior_interval_source != nullptr);
        
        // Verify they have different timeframes
        auto analog_tf = triangular_source->getTimeFrame();
        auto behavior_tf = behavior_interval_source->getTimeFrame();
        REQUIRE(analog_tf != behavior_tf);
        REQUIRE(analog_tf->getTotalFrameCount() == 101);  // analog_time: 0-100
        REQUIRE(behavior_tf->getTotalFrameCount() == 51);   // behavior_time: 0,2,4,...,100
        
        // Create a simple test with one behavior interval
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(15))  // Behavior period 1: analog time 10-30
        };
        
        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_tf);
        
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Add analog slice gatherer
        builder.addColumn<std::vector<double>>("TriangularSlice", 
            std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(triangular_source, "TriangularWave"));
        
        // Build and verify the table
        TableView table = builder.build();
        
        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 1);
        
        auto slices = table.getColumnValues<std::vector<double>>("TriangularSlice");
        
        REQUIRE(slices.size() == 1);
        REQUIRE(slices[0].size() > 0);
        
        // The slice should contain values from the triangular wave between time 10-30
        // These should be values from the rising edge (10->30 on a 0->50->0 triangular wave)
        for (size_t i = 0; i < slices[0].size(); ++i) {
            double value = slices[0][i];
            REQUIRE(value >= 10.0);
            REQUIRE(value <= 30.0);
        }
        
        std::cout << "Cross-timeframe test - Slice size: " << slices[0].size() 
                  << ", First value: " << slices[0][0] 
                  << ", Last value: " << slices[0][slices[0].size()-1] << std::endl;
    }
}

TEST_CASE_METHOD(AnalogSliceTableRegistryTestFixture, "DM - TV - AnalogSliceGathererComputer via ComputerRegistry", "[AnalogSliceGathererComputer][Registry]") {
    
    SECTION("Verify AnalogSliceGathererComputer is registered in ComputerRegistry") {
        auto& registry = getTableRegistry().getComputerRegistry();
        
        // Check that analog slice gatherer computers are registered
        auto double_info = registry.findComputerInfo("Analog Slice Gatherer");
        auto float_info = registry.findComputerInfo("Analog Slice Gatherer Float");
        
        REQUIRE(double_info != nullptr);
        REQUIRE(float_info != nullptr);
        
        // Verify computer info details for double version
        REQUIRE(double_info->name == "Analog Slice Gatherer");
        REQUIRE(double_info->outputType == typeid(std::vector<double>));
        REQUIRE(double_info->outputTypeName == "std::vector<double>");
        REQUIRE(double_info->isVectorType == true);
        REQUIRE(double_info->elementType == typeid(double));
        REQUIRE(double_info->elementTypeName == "double");
        REQUIRE(double_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(double_info->requiredSourceType == typeid(std::shared_ptr<IAnalogSource>));
        
        // Verify computer info details for float version
        REQUIRE(float_info->name == "Analog Slice Gatherer Float");
        REQUIRE(float_info->outputType == typeid(std::vector<float>));
        REQUIRE(float_info->outputTypeName == "std::vector<float>");
        REQUIRE(float_info->isVectorType == true);
        REQUIRE(float_info->elementType == typeid(float));
        REQUIRE(float_info->elementTypeName == "float");
        REQUIRE(float_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(float_info->requiredSourceType == typeid(std::shared_ptr<IAnalogSource>));
    }
    
    SECTION("Create AnalogSliceGathererComputer via ComputerRegistry") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto& registry = getTableRegistry().getComputerRegistry();
        
        // Get triangular wave source for testing
        auto triangular_source = dme->getAnalogSource("TriangularWave");
        REQUIRE(triangular_source != nullptr);
        
        // Create computers via registry
        std::map<std::string, std::string> empty_params;
        
        auto double_computer = registry.createTypedComputer<std::vector<double>>(
            "Analog Slice Gatherer", triangular_source, empty_params);
        auto float_computer = registry.createTypedComputer<std::vector<float>>(
            "Analog Slice Gatherer Float", triangular_source, empty_params);
        
        REQUIRE(double_computer != nullptr);
        REQUIRE(float_computer != nullptr);
        
        // Test that the created computers work correctly
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        
        // Create a simple test interval
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(20), TimeFrameIndex(30))  // Behavior period covering triangular peak
        };
        
        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_time_frame);
        
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Use the registry-created computers
        builder.addColumn("RegistryDoubleSlice", std::move(double_computer));
        builder.addColumn("RegistryFloatSlice", std::move(float_computer));
        
        // Build and verify the table
        TableView table = builder.build();
        
        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("RegistryDoubleSlice"));
        REQUIRE(table.hasColumn("RegistryFloatSlice"));
        
        auto double_slices = table.getColumnValues<std::vector<double>>("RegistryDoubleSlice");
        auto float_slices = table.getColumnValues<std::vector<float>>("RegistryFloatSlice");
        
        REQUIRE(double_slices.size() == 1);
        REQUIRE(float_slices.size() == 1);
        REQUIRE(double_slices[0].size() > 0);
        REQUIRE(float_slices[0].size() > 0);
        REQUIRE(double_slices[0].size() == float_slices[0].size());
        
        // Values should be from the triangular wave peak region (around 40-60)
        for (auto value : double_slices[0]) {
            REQUIRE(value >= 20.0);
            REQUIRE(value <= 50.0);
        }
        
        std::cout << "Registry test - Double slice size: " << double_slices[0].size() 
                  << ", Float slice size: " << float_slices[0].size() << std::endl;
    }
    
    SECTION("Compare registry-created vs direct-created computers") {
        auto& dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto& registry = getTableRegistry().getComputerRegistry();
        
        auto triangular_source = dme->getAnalogSource("TriangularWave");
        REQUIRE(triangular_source != nullptr);
        
        // Create computer via registry
        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<std::vector<double>>(
            "Analog Slice Gatherer", triangular_source, empty_params);
        
        // Create computer directly
        auto direct_computer = std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(
            triangular_source, "TriangularWave");
        
        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);
        
        // Test both computers with the same data
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(20), TimeFrameIndex(30))
        };
        
        ExecutionPlan plan(test_intervals, behavior_time_frame);
        
        auto [registry_result, registryEntity_ids] = registry_computer->compute(plan);
        auto [direct_result, directEntity_ids] = direct_computer->compute(plan);
        
        REQUIRE(registry_result.size() == 1);
        REQUIRE(direct_result.size() == 1);
        REQUIRE(registry_result[0].size() == direct_result[0].size());
        
        // Results should be identical
        for (size_t i = 0; i < registry_result[0].size(); ++i) {
            REQUIRE(registry_result[0][i] == Catch::Approx(direct_result[0][i]));
        }
        
        std::cout << "Comparison test - Registry result size: " << registry_result[0].size() 
                  << ", Direct result size: " << direct_result[0].size() << std::endl;
    }
}

TEST_CASE_METHOD(AnalogSliceTableRegistryTestFixture, "DM - TV - AnalogSliceGathererComputer via JSON TablePipeline", "[AnalogSliceGathererComputer][JSON][Pipeline]") {
    
    SECTION("Test double version via JSON pipeline") {
        // JSON configuration for testing AnalogSliceGathererComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Analog Slice Gatherer Test",
                "description": "Test JSON execution of AnalogSliceGathererComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "analog_slice_test",
                    "name": "Analog Slice Test Table",
                    "description": "Test table using AnalogSliceGathererComputer",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "TriangularSlices",
                            "description": "Triangular wave data slices during behavior periods",
                            "data_source": "TriangularWave",
                            "computer": "Analog Slice Gatherer"
                        },
                        {
                            "name": "SineSlices",
                            "description": "Sine wave data slices during behavior periods",
                            "data_source": "SineWave",
                            "computer": "Analog Slice Gatherer"
                        }
                    ]
                }
            ]
        })";

        auto& pipeline = getTablePipeline();

        // Parse the JSON configuration
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        // Load configuration into pipeline
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        // Verify configuration was loaded correctly
        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const& config = table_configs[0];
        REQUIRE(config.table_id == "analog_slice_test");
        REQUIRE(config.name == "Analog Slice Test Table");
        REQUIRE(config.columns.size() == 2);

        // Verify column configurations
        auto const& column1 = config.columns[0];
        REQUIRE(column1["name"] == "TriangularSlices");
        REQUIRE(column1["computer"] == "Analog Slice Gatherer");
        REQUIRE(column1["data_source"] == "TriangularWave");

        auto const& column2 = config.columns[1];
        REQUIRE(column2["name"] == "SineSlices");
        REQUIRE(column2["computer"] == "Analog Slice Gatherer");
        REQUIRE(column2["data_source"] == "SineWave");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "interval");
        REQUIRE(config.row_selector["source"] == "BehaviorPeriods");

        std::cout << "JSON pipeline configuration loaded and parsed successfully" << std::endl;

        // Execute the pipeline
        auto pipeline_result = pipeline.execute([](int table_index, std::string const& table_name, int table_progress, int overall_progress) {
            std::cout << "Building table " << table_index << " (" << table_name << "): "
                      << table_progress << "% (Overall: " << overall_progress << "%)" << std::endl;
        });

        if (pipeline_result.success) {
            std::cout << "Pipeline executed successfully!" << std::endl;
            std::cout << "Tables completed: " << pipeline_result.tables_completed << "/" << pipeline_result.total_tables << std::endl;
            std::cout << "Execution time: " << pipeline_result.total_execution_time_ms << " ms" << std::endl;

            // Verify the built table exists
            auto& registry = getTableRegistry();
            REQUIRE(registry.hasTable("analog_slice_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("analog_slice_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const& name : column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 2);
            REQUIRE(built_table->hasColumn("TriangularSlices"));
            REQUIRE(built_table->hasColumn("SineSlices"));

            // Verify table has 3 rows (one for each behavior period)
            REQUIRE(built_table->getRowCount() == 3);

            // Get and verify the computed values
            auto triangular_slices = built_table->getColumnValues<std::vector<double>>("TriangularSlices");
            auto sine_slices = built_table->getColumnValues<std::vector<double>>("SineSlices");

            REQUIRE(triangular_slices.size() == 3);
            REQUIRE(sine_slices.size() == 3);

            for (size_t i = 0; i < 3; ++i) {
                REQUIRE(triangular_slices[i].size() > 0);
                REQUIRE(sine_slices[i].size() > 0);
                
                // Triangular wave values should be in range [0, 50]
                for (auto value : triangular_slices[i]) {
                    REQUIRE(value >= 0.0);
                    REQUIRE(value <= 50.0);
                }
                
                // Sine wave values should be in range [-25, 25]
                for (auto value : sine_slices[i]) {
                    REQUIRE(value >= -25.0);
                    REQUIRE(value <= 25.0);
                }
                
                std::cout << "Row " << i << ": Triangular slice size=" << triangular_slices[i].size() 
                          << ", Sine slice size=" << sine_slices[i].size() << std::endl;
            }

        } else {
            std::cout << "Pipeline execution failed: " << pipeline_result.error_message << std::endl;
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }
    
    SECTION("Test float version via JSON pipeline") {
        char const * json_config = R"({
            "metadata": {
                "name": "Analog Slice Gatherer Float Test",
                "description": "Test JSON execution of AnalogSliceGathererComputer float version"
            },
            "tables": [
                {
                    "table_id": "analog_slice_float_test",
                    "name": "Analog Slice Float Test Table",
                    "description": "Test table using AnalogSliceGathererComputer float version",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "TriangularSlicesFloat",
                            "description": "Triangular wave data slices as floats",
                            "data_source": "TriangularWave",
                            "computer": "Analog Slice Gatherer Float"
                        }
                    ]
                }
            ]
        })";

        auto& pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);
        
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);
        
        auto const& config = table_configs[0];
        REQUIRE(config.columns.size() == 1);
        REQUIRE(config.columns[0]["computer"] == "Analog Slice Gatherer Float");

        std::cout << "Float version JSON configuration parsed successfully" << std::endl;
        
        auto pipeline_result = pipeline.execute();
        
        if (pipeline_result.success) {
            std::cout << "✓ Float version pipeline executed successfully!" << std::endl;
            
            auto& registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("analog_slice_float_test");
            REQUIRE(built_table != nullptr);
            
            REQUIRE(built_table->getRowCount() == 3);
            REQUIRE(built_table->getColumnCount() == 1);
            REQUIRE(built_table->hasColumn("TriangularSlicesFloat"));
            
            auto float_slices = built_table->getColumnValues<std::vector<float>>("TriangularSlicesFloat");
            REQUIRE(float_slices.size() == 3);
            
            // Verify all slices are reasonable
            for (size_t i = 0; i < 3; ++i) {
                REQUIRE(float_slices[i].size() > 0);
                for (auto value : float_slices[i]) {
                    REQUIRE(value >= 0.0f);
                    REQUIRE(value <= 50.0f);
                }
                std::cout << "Behavior period " << i << ": " << float_slices[i].size() << " samples" << std::endl;
            }
            
        } else {
            FAIL("Float version pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}
