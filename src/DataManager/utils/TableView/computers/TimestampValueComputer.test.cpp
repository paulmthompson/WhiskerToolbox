#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimestampValueComputer.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IAnalogSource.h"

// Additional includes for extended testing
#include "DataManager.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <vector>

/**
 * @brief Base test fixture for TimestampValueComputer with realistic analog data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Analog signals with known patterns for predictable value extraction
 * - Cross-timeframe timestamp sampling for testing timeframe conversion
 */
class TimestampValueTestFixture {
protected:
    TimestampValueTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();

        // Populate with test data
        populateWithAnalogTestData();
    }

    ~TimestampValueTestFixture() = default;

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
    }

    /**
     * @brief Create TimeFrame objects for different data streams
     */
    void createTimeFrames() {
        // Create "behavior_time" timeframe: 0 to 100 (101 points) - behavior tracking at 10Hz
        std::vector<int> behavior_time_values(101);
        std::iota(behavior_time_values.begin(), behavior_time_values.end(), 0);
        auto behavior_time_frame = std::make_shared<TimeFrame>(behavior_time_values);
        m_data_manager->setTime(TimeKey("behavior_time"), behavior_time_frame, true);

        // Create "signal_time" timeframe: 0, 2, 4, 6, ..., 100 (51 points) - signal at 5Hz
        std::vector<int> signal_time_values;
        signal_time_values.reserve(51);
        for (int i = 0; i <= 50; ++i) {
            signal_time_values.push_back(i * 2);
        }
        auto signal_time_frame = std::make_shared<TimeFrame>(signal_time_values);
        m_data_manager->setTime(TimeKey("signal_time"), signal_time_frame, true);

        // Create "high_res_time" timeframe: 0 to 100 in 1-unit steps (101 points) - high resolution
        std::vector<int> high_res_time_values;
        high_res_time_values.reserve(101);
        for (int i = 0; i <= 100; ++i) {
            high_res_time_values.push_back(i);
        }
        auto high_res_time_frame = std::make_shared<TimeFrame>(high_res_time_values);
        m_data_manager->setTime(TimeKey("high_res_time"), high_res_time_frame, true);
    }

    /**
     * @brief Create analog signals with known patterns for testing value extraction
     */
    void createAnalogSignals() {
        // Create Linear Signal: values 0, 1, 2, 3, ... 50 (matches signal_time indices)
        {
            std::vector<float> linear_data(51);
            std::iota(linear_data.begin(), linear_data.end(), 0.0f);
            auto linear_signal = std::make_shared<AnalogTimeSeries>(std::move(linear_data), 51);
            m_data_manager->setData<AnalogTimeSeries>("LinearSignal", linear_signal, TimeKey("signal_time"));
        }

        // Create Sine Wave Signal: sin(2*pi*t/20) for high resolution sampling
        {
            std::vector<float> sine_data(101);
            for (size_t i = 0; i < 101; ++i) {
                double t = static_cast<double>(i);// time value (now integer-based)
                sine_data[i] = static_cast<float>(std::sin(2.0 * M_PI * t / 20.0));
            }
            auto sine_signal = std::make_shared<AnalogTimeSeries>(std::move(sine_data), 101);
            m_data_manager->setData<AnalogTimeSeries>("SineWave", sine_signal, TimeKey("high_res_time"));
        }

        // Create Square Wave Signal: alternating 1.0 and -1.0 every 10 time units
        {
            std::vector<float> square_data(101);
            for (size_t i = 0; i < 101; ++i) {
                int time_value = static_cast<int>(i);
                square_data[i] = ((time_value / 10) % 2 == 0) ? 1.0f : -1.0f;
            }
            auto square_signal = std::make_shared<AnalogTimeSeries>(std::move(square_data), 101);
            m_data_manager->setData<AnalogTimeSeries>("SquareWave", square_signal, TimeKey("behavior_time"));
        }

        // Create Noise Signal: pseudo-random values for edge case testing
        {
            std::vector<float> noise_data(101);
            std::srand(42);// Fixed seed for reproducible tests
            for (size_t i = 0; i < 101; ++i) {
                noise_data[i] = static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f;// Range [-1, 1]
            }
            auto noise_signal = std::make_shared<AnalogTimeSeries>(std::move(noise_data), 101);
            m_data_manager->setData<AnalogTimeSeries>("NoiseSignal", noise_signal, TimeKey("behavior_time"));
        }
    }
};

/**
 * @brief Test fixture combining TimestampValueTestFixture with TableRegistry and TablePipeline
 * 
 * This fixture provides everything needed to test JSON-based table pipeline execution:
 * - DataManager with analog test data (from TimestampValueTestFixture)
 * - TableRegistry for managing table configurations
 * - TablePipeline for executing JSON configurations
 */
class TimestampValueTableRegistryTestFixture : public TimestampValueTestFixture {
protected:
    TimestampValueTableRegistryTestFixture()
        : TimestampValueTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~TimestampValueTableRegistryTestFixture() = default;

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
    TableRegistry * m_table_registry_ptr;// Points to DataManager's TableRegistry
    std::unique_ptr<TablePipeline> m_table_pipeline;
    std::shared_ptr<DataManagerExtension> m_data_manager_extension;// Lazy-initialized
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

    auto getData() -> std::span<float const> {
        return std::span<float const>(m_data);
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

TEST_CASE("DM - TV - TimestampValueComputer Basic Functionality", "[TimestampValueComputer]") {

    SECTION("Basic value extraction at timestamps") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create analog data with known values
        std::vector<float> analogData = {10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Create timestamps for value extraction
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(0),// Should extract value 10.0
                TimeFrameIndex(3),// Should extract value 13.0
                TimeFrameIndex(7),// Should extract value 17.0
                TimeFrameIndex(9) // Should extract value 19.0
        };

        ExecutionPlan plan(timestamps, timeFrame);

        // Create the computer
        TimestampValueComputer computer(analogSource);

        // Compute the results
        auto results = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 4);
        REQUIRE(results[0] == Catch::Approx(10.0));
        REQUIRE(results[1] == Catch::Approx(13.0));
        REQUIRE(results[2] == Catch::Approx(17.0));
        REQUIRE(results[3] == Catch::Approx(19.0));
    }

    SECTION("Edge case: single timestamp") {
        // Create time frame
        std::vector<int> timeValues = {0, 10, 20, 30};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create analog data
        std::vector<float> analogData = {1.5f, 2.5f, 3.5f, 4.5f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Single timestamp
        std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(2)};

        ExecutionPlan plan(timestamps, timeFrame);

        // Create the computer
        TimestampValueComputer computer(analogSource);

        // Compute the results
        auto results = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == Catch::Approx(3.5));
    }

    SECTION("Edge case: empty data handling") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create analog source with empty data
        std::vector<float> analogData;// Empty data
        auto analogSource = std::make_shared<MockAnalogSource>("EmptySignal", timeFrame, analogData);

        // Create timestamps
        std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1)};

        ExecutionPlan plan(timestamps, timeFrame);

        // Create the computer
        TimestampValueComputer computer(analogSource);

        // Compute the results
        auto results = computer.compute(plan);

        // Verify results - should return NaN for missing data
        REQUIRE(results.size() == 2);
        REQUIRE(std::isnan(results[0]));
        REQUIRE(std::isnan(results[1]));
    }

    SECTION("Boundary timestamp handling") {
        // Create time frame
        std::vector<int> timeValues = {0, 10, 20, 30, 40};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create analog data
        std::vector<float> analogData = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Test boundary timestamps
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(0),// First timestamp
                TimeFrameIndex(4) // Last timestamp
        };

        ExecutionPlan plan(timestamps, timeFrame);

        // Create the computer
        TimestampValueComputer computer(analogSource);

        // Compute the results
        auto results = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 2);
        REQUIRE(results[0] == Catch::Approx(100.0));
        REQUIRE(results[1] == Catch::Approx(500.0));
    }

    SECTION("Custom source name constructor") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Create computer with custom source name
        TimestampValueComputer computer(analogSource, "CustomSourceName");

        // Test source dependency
        REQUIRE(computer.getSourceDependency() == "CustomSourceName");

        // Test that it still computes correctly
        std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(1)};
        ExecutionPlan plan(timestamps, timeFrame);

        auto results = computer.compute(plan);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == Catch::Approx(2.0));
    }
}

TEST_CASE("DM - TV - TimestampValueComputer Error Handling", "[TimestampValueComputer][Error]") {

    SECTION("Null source throws exception") {
        // Should throw when creating with null source
        REQUIRE_THROWS_AS(TimestampValueComputer(nullptr), std::invalid_argument);
        REQUIRE_THROWS_AS(TimestampValueComputer(nullptr, "test"), std::invalid_argument);
    }

    SECTION("ExecutionPlan without indices throws exception") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Create execution plan with intervals instead of indices
        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(1))};
        ExecutionPlan plan(intervals, timeFrame);

        // Create the computer
        TimestampValueComputer computer(analogSource);

        // Should throw an exception
        REQUIRE_THROWS_AS(computer.compute(plan), std::runtime_error);
    }

    SECTION("ExecutionPlan with null TimeFrame throws exception") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Create execution plan with timestamps but null timeframe
        std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0)};
        ExecutionPlan plan(timestamps, nullptr);

        // Create the computer
        TimestampValueComputer computer(analogSource);

        // Should throw an exception
        REQUIRE_THROWS_AS(computer.compute(plan), std::runtime_error);
    }
}

TEST_CASE("DM - TV - TimestampValueComputer Dependency Tracking", "[TimestampValueComputer][Dependencies]") {

    SECTION("getSourceDependency returns correct source name") {
        // Create minimal setup
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSource", timeFrame, analogData);

        // Test default source name (from source)
        TimestampValueComputer computer1(analogSource);
        REQUIRE(computer1.getSourceDependency() == "TestSource");

        // Test custom source name
        TimestampValueComputer computer2(analogSource, "CustomName");
        REQUIRE(computer2.getSourceDependency() == "CustomName");
    }
}

TEST_CASE_METHOD(TimestampValueTestFixture, "DM - TV - TimestampValueComputer with DataManager fixture", "[TimestampValueComputer][DataManager][Fixture]") {

    SECTION("Test with linear signal from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the analog source from the DataManager
        auto linear_source = dme->getAnalogSource("LinearSignal");
        REQUIRE(linear_source != nullptr);

        // Get the signal time frame
        auto signal_time_frame = dm.getTime(TimeKey("signal_time"));
        REQUIRE(signal_time_frame != nullptr);

        // Create timestamps for sampling (signal_time: 0, 2, 4, 6, ..., 100)
        // Sample at indices corresponding to times 0, 10, 20, 30
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(0), // time=0, signal index=0, value=0
                TimeFrameIndex(5), // time=10, signal index=5, value=5
                TimeFrameIndex(10),// time=20, signal index=10, value=10
                TimeFrameIndex(15) // time=30, signal index=15, value=15
        };

        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, signal_time_frame);

        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add TimestampValueComputer column
        builder.addColumn<double>("LinearValues",
                                  std::make_unique<TimestampValueComputer>(linear_source, "LinearSignal"));

        // Build the table
        TableView table = builder.build();

        // Verify table structure
        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 1);
        REQUIRE(table.hasColumn("LinearValues"));

        // Get the column data
        auto linear_values = table.getColumnValues<double>("LinearValues");

        REQUIRE(linear_values.size() == 4);

        // Verify the extracted values match expected pattern
        REQUIRE(linear_values[0] == Catch::Approx(0.0)); // signal[0] = 0
        REQUIRE(linear_values[1] == Catch::Approx(5.0)); // signal[5] = 5
        REQUIRE(linear_values[2] == Catch::Approx(10.0));// signal[10] = 10
        REQUIRE(linear_values[3] == Catch::Approx(15.0));// signal[15] = 15
    }

    SECTION("Test with sine wave signal from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the sine wave source
        auto sine_source = dme->getAnalogSource("SineWave");
        REQUIRE(sine_source != nullptr);

        // Get the high resolution time frame
        auto high_res_time_frame = dm.getTime(TimeKey("high_res_time"));
        REQUIRE(high_res_time_frame != nullptr);

        // Sample at known sine wave points (high_res_time: 0, 1, 2, ..., 100)
        // Period = 20, so at t=0, 5, 10, 15, 20 we get known sine values
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(0), // t=0, sin(0) = 0
                TimeFrameIndex(5), // t=5, sin(π/2) = 1
                TimeFrameIndex(10),// t=10, sin(π) = 0
                TimeFrameIndex(15),// t=15, sin(3π/2) = -1
                TimeFrameIndex(20) // t=20, sin(2π) = 0
        };

        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, high_res_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        builder.addColumn<double>("SineValues",
                                  std::make_unique<TimestampValueComputer>(sine_source, "SineWave"));

        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 5);
        REQUIRE(table.hasColumn("SineValues"));

        auto sine_values = table.getColumnValues<double>("SineValues");
        REQUIRE(sine_values.size() == 5);

        // Verify sine wave values (with some tolerance for floating point)
        REQUIRE(sine_values[0] == Catch::Approx(0.0).margin(0.01)); // sin(0)
        REQUIRE(sine_values[1] == Catch::Approx(1.0).margin(0.01)); // sin(π/2)
        REQUIRE(sine_values[2] == Catch::Approx(0.0).margin(0.01)); // sin(π)
        REQUIRE(sine_values[3] == Catch::Approx(-1.0).margin(0.01));// sin(3π/2)
        REQUIRE(sine_values[4] == Catch::Approx(0.0).margin(0.01)); // sin(2π)
    }


}

TEST_CASE_METHOD(TimestampValueTableRegistryTestFixture, "DM - TV - TimestampValueComputer via ComputerRegistry", "[TimestampValueComputer][Registry]") {

    SECTION("Verify TimestampValueComputer is registered in ComputerRegistry") {
        auto & registry = getTableRegistry().getComputerRegistry();

        // Check that TimestampValueComputer is registered
        auto computer_info = registry.findComputerInfo("Timestamp Value");

        REQUIRE(computer_info != nullptr);

        // Verify computer info details
        REQUIRE(computer_info->name == "Timestamp Value");
        REQUIRE(computer_info->outputType == typeid(double));
        REQUIRE(computer_info->outputTypeName == "double");
        REQUIRE(computer_info->requiredRowSelector == RowSelectorType::Timestamp);
        REQUIRE(computer_info->requiredSourceType == typeid(std::shared_ptr<IAnalogSource>));
    }

    SECTION("Create TimestampValueComputer via ComputerRegistry") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        // Get analog source for testing
        auto linear_source = dme->getAnalogSource("LinearSignal");
        REQUIRE(linear_source != nullptr);

        // Create computer via registry
        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<double>(
                "Timestamp Value", linear_source, empty_params);

        REQUIRE(registry_computer != nullptr);

        // Test that the created computer works correctly
        auto signal_time_frame = dm.getTime(TimeKey("signal_time"));

        // Create a simple test with known values
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(0), // Should extract value 0
                TimeFrameIndex(10),// Should extract value 10
                TimeFrameIndex(25) // Should extract value 25
        };

        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, signal_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Use the registry-created computer
        builder.addColumn("RegistryTimestampValues", std::move(registry_computer));

        // Build and verify the table
        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 3);
        REQUIRE(table.getColumnCount() == 1);
        REQUIRE(table.hasColumn("RegistryTimestampValues"));

        auto values = table.getColumnValues<double>("RegistryTimestampValues");

        REQUIRE(values.size() == 3);

        // Verify reasonable results
        REQUIRE(values[0] == Catch::Approx(0.0));
        REQUIRE(values[1] == Catch::Approx(10.0));
        REQUIRE(values[2] == Catch::Approx(25.0));

        std::cout << "Registry test - Values: " << values[0]
                  << ", " << values[1] << ", " << values[2] << std::endl;
    }

    SECTION("Compare registry-created vs direct-created computers") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        auto linear_source = dme->getAnalogSource("LinearSignal");
        REQUIRE(linear_source != nullptr);

        // Create computer via registry
        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<double>(
                "Timestamp Value", linear_source, empty_params);

        // Create computer directly
        auto direct_computer = std::make_unique<TimestampValueComputer>(linear_source, "LinearSignal");

        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);

        // Test both computers with the same data
        auto signal_time_frame = dm.getTime(TimeKey("signal_time"));
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(5),// Should extract value 5
                TimeFrameIndex(15)// Should extract value 15
        };

        ExecutionPlan plan(test_timestamps, signal_time_frame);

        auto registry_result = registry_computer->compute(plan);
        auto direct_result = direct_computer->compute(plan);

        REQUIRE(registry_result.size() == 2);
        REQUIRE(direct_result.size() == 2);

        // Results should be identical
        REQUIRE(registry_result[0] == Catch::Approx(direct_result[0]));
        REQUIRE(registry_result[1] == Catch::Approx(direct_result[1]));

        std::cout << "Comparison test - Registry result: " << registry_result[0]
                  << ", " << registry_result[1] << " | Direct result: " << direct_result[0]
                  << ", " << direct_result[1] << std::endl;
    }
}

TEST_CASE_METHOD(TimestampValueTableRegistryTestFixture, "DM - TV - TimestampValueComputer via JSON TablePipeline", "[TimestampValueComputer][JSON][Pipeline]") {

    SECTION("Test basic timestamp value extraction via JSON pipeline") {
        // JSON configuration for testing TimestampValueComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Timestamp Value Extraction Test",
                "description": "Test JSON execution of TimestampValueComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "timestamp_value_test",
                    "name": "Timestamp Value Test Table",
                    "description": "Test table using TimestampValueComputer",
                    "row_selector": {
                        "type": "timestamp",
                        "timestamps": [0, 10, 20, 30, 40]
                    },
                    "columns": [
                        {
                            "name": "LinearSignalValues",
                            "description": "Values from linear signal at specific timestamps",
                            "data_source": "LinearSignal",
                            "computer": "Timestamp Value"
                        },
                        {
                            "name": "SquareWaveValues",
                            "description": "Values from square wave signal at specific timestamps",
                            "data_source": "SquareWave",
                            "computer": "Timestamp Value"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();

        // Parse the JSON configuration
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        // Load configuration into pipeline
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        // Verify configuration was loaded correctly
        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const & config = table_configs[0];
        REQUIRE(config.table_id == "timestamp_value_test");
        REQUIRE(config.name == "Timestamp Value Test Table");
        REQUIRE(config.columns.size() == 2);

        // Verify column configurations
        auto const & column1 = config.columns[0];
        REQUIRE(column1["name"] == "LinearSignalValues");
        REQUIRE(column1["computer"] == "Timestamp Value");
        REQUIRE(column1["data_source"] == "LinearSignal");

        auto const & column2 = config.columns[1];
        REQUIRE(column2["name"] == "SquareWaveValues");
        REQUIRE(column2["computer"] == "Timestamp Value");
        REQUIRE(column2["data_source"] == "SquareWave");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "timestamp");
        REQUIRE(config.row_selector["timestamps"].is_array());
        REQUIRE(config.row_selector["timestamps"].size() == 5);

        std::cout << "JSON pipeline configuration loaded and parsed successfully" << std::endl;

        // Execute the pipeline
        auto pipeline_result = pipeline.execute([](int table_index, std::string const & table_name, int table_progress, int overall_progress) {
            std::cout << "Building table " << table_index << " (" << table_name << "): "
                      << table_progress << "% (Overall: " << overall_progress << "%)" << std::endl;
        });

        if (pipeline_result.success) {
            std::cout << "Pipeline executed successfully!" << std::endl;
            std::cout << "Tables completed: " << pipeline_result.tables_completed << "/" << pipeline_result.total_tables << std::endl;
            std::cout << "Execution time: " << pipeline_result.total_execution_time_ms << " ms" << std::endl;

            // Verify the built table exists
            auto & registry = getTableRegistry();
            REQUIRE(registry.hasTable("timestamp_value_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("timestamp_value_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const & name: column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 2);
            REQUIRE(built_table->hasColumn("LinearSignalValues"));
            REQUIRE(built_table->hasColumn("SquareWaveValues"));

            // Verify table has 5 rows (one for each timestamp)
            REQUIRE(built_table->getRowCount() == 5);

            // Get and verify the computed values
            auto linear_values = built_table->getColumnValues<double>("LinearSignalValues");
            auto square_values = built_table->getColumnValues<double>("SquareWaveValues");

            REQUIRE(linear_values.size() == 5);
            REQUIRE(square_values.size() == 5);

            for (size_t i = 0; i < 5; ++i) {
                // Linear signal values should be reasonable
                REQUIRE(linear_values[i] >= 0.0);

                // Square wave values should be either 1.0 or -1.0
                REQUIRE((square_values[i] == Catch::Approx(1.0) || square_values[i] == Catch::Approx(-1.0)));

                std::cout << "Row " << i << ": Linear=" << linear_values[i]
                          << ", Square=" << square_values[i] << std::endl;
            }

        } else {
            std::cout << "Pipeline execution failed: " << pipeline_result.error_message << std::endl;
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test timestamp value extraction with high-resolution signal") {
        char const * json_config = R"({
            "metadata": {
                "name": "High-Resolution Timestamp Test", 
                "description": "Test TimestampValueComputer with high-resolution signals"
            },
            "tables": [
                {
                    "table_id": "high_res_timestamp_test",
                    "name": "High Resolution Timestamp Test Table",
                    "description": "Test table using TimestampValueComputer on sine wave",
                    "row_selector": {
                        "type": "timestamp",
                        "source": "high_res_time"
                    },
                    "columns": [
                        {
                            "name": "SineWaveValues",
                            "description": "Values from sine wave at specific timestamps",
                            "data_source": "SineWave",
                            "computer": "Timestamp Value"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const & config = table_configs[0];
        REQUIRE(config.columns.size() == 1);
        REQUIRE(config.columns[0]["computer"] == "Timestamp Value");
        REQUIRE(config.columns[0]["data_source"] == "SineWave");
        REQUIRE(config.row_selector["source"] == "high_res_time");

        std::cout << "High-resolution JSON configuration parsed successfully" << std::endl;

        auto pipeline_result = pipeline.execute();

        if (pipeline_result.success) {
            std::cout << "✓ High-resolution pipeline executed successfully!" << std::endl;

            auto & registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("high_res_timestamp_test");
            REQUIRE(built_table != nullptr);

            // Since we're using "high_res_time" source, we should have 101 rows (0-100)
            REQUIRE(built_table->getRowCount() == 101);
            REQUIRE(built_table->getColumnCount() == 1);
            REQUIRE(built_table->hasColumn("SineWaveValues"));

            auto sine_values = built_table->getColumnValues<double>("SineWaveValues");
            REQUIRE(sine_values.size() == 101);

            // Verify key sine wave values at known points (period = 20)
            // At t=0, 5, 10, 15, 20 we expect sin(0), sin(π/2), sin(π), sin(3π/2), sin(2π)
            REQUIRE(sine_values[0] == Catch::Approx(0.0).margin(0.01));  // t=0, sin(0) = 0
            REQUIRE(sine_values[5] == Catch::Approx(1.0).margin(0.01));  // t=5, sin(π/2) = 1
            REQUIRE(sine_values[10] == Catch::Approx(0.0).margin(0.01)); // t=10, sin(π) = 0
            REQUIRE(sine_values[15] == Catch::Approx(-1.0).margin(0.01));// t=15, sin(3π/2) = -1
            REQUIRE(sine_values[20] == Catch::Approx(0.0).margin(0.01)); // t=20, sin(2π) = 0

            // Test some additional points to ensure the pattern continues
            REQUIRE(sine_values[25] == Catch::Approx(1.0).margin(0.01)); // t=25, sin(5π/2) = 1
            REQUIRE(sine_values[40] == Catch::Approx(0.0).margin(0.01)); // t=40, sin(4π) = 0
            
            std::cout << "Verified sine wave values at key points:" << std::endl;
            std::cout << "  t=0: " << sine_values[0] << " (expected ~0)" << std::endl;
            std::cout << "  t=5: " << sine_values[5] << " (expected ~1)" << std::endl;
            std::cout << "  t=10: " << sine_values[10] << " (expected ~0)" << std::endl;
            std::cout << "  t=15: " << sine_values[15] << " (expected ~-1)" << std::endl;
            std::cout << "  t=20: " << sine_values[20] << " (expected ~0)" << std::endl;

        } else {
            FAIL("High-resolution pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test specific timestamp extraction with correct timeframe") {
        // This test specifically addresses the cross-timeframe issue by ensuring
        // we're using the correct timeframe for timestamp specification
        char const * json_config = R"({
            "metadata": {
                "name": "Specific Timestamp Test", 
                "description": "Test TimestampValueComputer with specific timestamps and correct timeframe"
            },
            "tables": [
                {
                    "table_id": "specific_timestamp_test",
                    "name": "Specific Timestamp Test Table",
                    "description": "Test table using specific timestamps with behavior time",
                    "row_selector": {
                        "type": "timestamp",
                        "source": "behavior_time"
                    },
                    "columns": [
                        {
                            "name": "SquareWaveAtBehaviorTime",
                            "description": "Square wave values at behavior timestamps",
                            "data_source": "SquareWave", 
                            "computer": "Timestamp Value"
                        },
                        {
                            "name": "LinearAtBehaviorTime",
                            "description": "Linear signal values at behavior timestamps", 
                            "data_source": "LinearSignal",
                            "computer": "Timestamp Value"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);
        
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto pipeline_result = pipeline.execute();
        
        if (pipeline_result.success) {
            std::cout << "✓ Specific timestamp pipeline executed successfully!" << std::endl;
            
            auto & registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("specific_timestamp_test");
            REQUIRE(built_table != nullptr);
            
            // Should have 101 rows (behavior_time: 0-100)
            REQUIRE(built_table->getRowCount() == 101);
            REQUIRE(built_table->getColumnCount() == 2);
            REQUIRE(built_table->hasColumn("SquareWaveAtBehaviorTime"));
            REQUIRE(built_table->hasColumn("LinearAtBehaviorTime"));
            
            auto square_values = built_table->getColumnValues<double>("SquareWaveAtBehaviorTime");
            auto linear_values = built_table->getColumnValues<double>("LinearAtBehaviorTime");
            
            REQUIRE(square_values.size() == 101);
            REQUIRE(linear_values.size() == 101);
            
            // Test specific known values
            // Square wave: alternating 1.0 and -1.0 every 10 time units
            REQUIRE(square_values[0] == Catch::Approx(1.0));   // t=0, period [0-10) = 1.0
            REQUIRE(square_values[5] == Catch::Approx(1.0));   // t=5, period [0-10) = 1.0  
            REQUIRE(square_values[10] == Catch::Approx(-1.0)); // t=10, period [10-20) = -1.0
            REQUIRE(square_values[15] == Catch::Approx(-1.0)); // t=15, period [10-20) = -1.0
            REQUIRE(square_values[20] == Catch::Approx(1.0));  // t=20, period [20-30) = 1.0
            
            // Linear signal values should show cross-timeframe conversion is working
            // Since LinearSignal is on signal_time (0,2,4,6,...,100) and we're sampling 
            // at behavior_time (0,1,2,3,...,100), we should get reasonable interpolated values
            REQUIRE(std::isfinite(linear_values[0]));
            REQUIRE(std::isfinite(linear_values[10])); 
            REQUIRE(std::isfinite(linear_values[50]));
            
            std::cout << "Cross-timeframe sampling verification:" << std::endl;
            std::cout << "  Square[0]: " << square_values[0] << " (expected 1.0)" << std::endl;
            std::cout << "  Square[10]: " << square_values[10] << " (expected -1.0)" << std::endl;
            std::cout << "  Linear[0]: " << linear_values[0] << " (should be finite)" << std::endl;
            std::cout << "  Linear[10]: " << linear_values[10] << " (should be finite)" << std::endl;
            
        } else {
            FAIL("Specific timestamp pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test error handling with missing data source") {
        char const * json_config = R"({
            "metadata": {
                "name": "Missing Source Test",
                "description": "Test error handling with missing data source"
            },
            "tables": [
                {
                    "table_id": "missing_source_test",
                    "name": "Missing Source Test Table",
                    "description": "Test table with non-existent data source",
                    "row_selector": {
                        "type": "timestamp",
                        "timestamps": [0, 10, 20]
                    },
                    "columns": [
                        {
                            "name": "NonExistentValues",
                            "description": "Values from non-existent signal",
                            "data_source": "NonExistentSignal",
                            "computer": "Timestamp Value"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        // Execute the pipeline - should fail gracefully
        auto pipeline_result = pipeline.execute();

        // Pipeline should fail with a meaningful error message
        REQUIRE(!pipeline_result.success);
        REQUIRE(!pipeline_result.error_message.empty());

        std::cout << "✓ Expected error handling worked: " << pipeline_result.error_message << std::endl;
    }
}

TEST_CASE_METHOD(TimestampValueTableRegistryTestFixture, "DM - TV - TimestampValueComputer JSON multi-source configuration", "[TimestampValueComputer][JSON][MultiSource]") {

    SECTION("Test timestamp extraction from multiple signals with different timeframes") {
        char const * json_config = R"({
            "metadata": {
                "name": "Multi-Source Timestamp Test",
                "description": "Test TimestampValueComputer with multiple signals from different timeframes"
            },
            "tables": [
                {
                    "table_id": "multi_source_timestamp_test",
                    "name": "Multi-Source Timestamp Test Table",
                    "description": "Extract values from multiple signals at the same timestamps",
                    "row_selector": {
                        "type": "timestamp",
                        "timestamps": [0, 20, 40, 60, 80]
                    },
                    "columns": [
                        {
                            "name": "LinearSignal",
                            "description": "Linear signal values",
                            "data_source": "LinearSignal",
                            "computer": "Timestamp Value"
                        },
                        {
                            "name": "SquareWave",
                            "description": "Square wave values",
                            "data_source": "SquareWave",
                            "computer": "Timestamp Value"
                        },
                        {
                            "name": "SineWave",
                            "description": "Sine wave values",
                            "data_source": "SineWave",
                            "computer": "Timestamp Value"
                        },
                        {
                            "name": "NoiseSignal",
                            "description": "Noise signal values",
                            "data_source": "NoiseSignal",
                            "computer": "Timestamp Value"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const & config = table_configs[0];
        REQUIRE(config.columns.size() == 4);

        // Verify all columns use TimestampValueComputer
        for (auto const & column: config.columns) {
            REQUIRE(column["computer"] == "Timestamp Value");
        }

        std::cout << "Multi-source JSON configuration parsed successfully" << std::endl;

        auto pipeline_result = pipeline.execute();

        if (pipeline_result.success) {
            std::cout << "✓ Multi-source pipeline executed successfully!" << std::endl;

            auto & registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("multi_source_timestamp_test");
            REQUIRE(built_table != nullptr);

            REQUIRE(built_table->getRowCount() == 5);
            REQUIRE(built_table->getColumnCount() == 4);

            // Verify all columns exist
            REQUIRE(built_table->hasColumn("LinearSignal"));
            REQUIRE(built_table->hasColumn("SquareWave"));
            REQUIRE(built_table->hasColumn("SineWave"));
            REQUIRE(built_table->hasColumn("NoiseSignal"));

            // Get all column data
            auto linear_values = built_table->getColumnValues<double>("LinearSignal");
            auto square_values = built_table->getColumnValues<double>("SquareWave");
            auto sine_values = built_table->getColumnValues<double>("SineWave");
            auto noise_values = built_table->getColumnValues<double>("NoiseSignal");

            REQUIRE(linear_values.size() == 5);
            REQUIRE(square_values.size() == 5);
            REQUIRE(sine_values.size() == 5);
            REQUIRE(noise_values.size() == 5);

            // Verify data consistency across all signals
            for (size_t i = 0; i < 5; ++i) {
                // All values should be finite (not NaN or infinite)
                REQUIRE(std::isfinite(linear_values[i]));
                REQUIRE(std::isfinite(square_values[i]));
                REQUIRE(std::isfinite(sine_values[i]));
                REQUIRE(std::isfinite(noise_values[i]));

                // Square wave should be either 1.0 or -1.0
                REQUIRE((square_values[i] == Catch::Approx(1.0) || square_values[i] == Catch::Approx(-1.0)));

                // Sine wave should be in [-1, 1]
                REQUIRE(sine_values[i] >= -1.0);
                REQUIRE(sine_values[i] <= 1.0);

                // Noise should be in [-1, 1] (our test range)
                REQUIRE(noise_values[i] >= -1.0);
                REQUIRE(noise_values[i] <= 1.0);

                std::cout << "Row " << i << ": Linear=" << linear_values[i]
                          << ", Square=" << square_values[i]
                          << ", Sine=" << sine_values[i]
                          << ", Noise=" << noise_values[i] << std::endl;
            }

        } else {
            FAIL("Multi-source pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}
