#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "IntervalReductionComputer.h"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IAnalogSource.h"

// Additional includes for extended testing
#include "DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <span>
#include <vector>

/**
 * @brief Base test fixture for IntervalReductionComputer with realistic analog data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Analog time series data for various signals
 * - Interval data for testing reduction operations
 */
class IntervalReductionTestFixture {
protected:
    IntervalReductionTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();

        // Populate with test data
        populateWithAnalogTestData();
    }

    ~IntervalReductionTestFixture() = default;

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
    }

    /**
     * @brief Create analog signal data for testing reductions
     */
    void createAnalogSignals() {
        // Create Linear Signal: values 1, 2, 3, 4, ... 51
        {
            std::vector<float> linear_data(51);
            std::iota(linear_data.begin(), linear_data.end(), 1.0f);
            auto linear_signal = std::make_shared<AnalogTimeSeries>(std::move(linear_data), linear_data.size());
            m_data_manager->setData<AnalogTimeSeries>("LinearSignal", linear_signal, TimeKey("signal_time"));
        }

        // Create Sine Wave Signal: sin(i * pi / 10) for i = 0 to 50
        {
            std::vector<float> sine_data(51);
            std::generate(sine_data.begin(), sine_data.end(), [i = 0]() mutable {
                return std::sin(static_cast<float>(i++) * M_PI / 10.0f);
            });
            auto sine_signal = std::make_shared<AnalogTimeSeries>(std::move(sine_data), sine_data.size());
            m_data_manager->setData<AnalogTimeSeries>("SineSignal", sine_signal, TimeKey("signal_time"));
        }

        // Create Constant Signal: all values = 5.0
        {
            std::vector<float> constant_data(51, 5.0f);
            auto constant_signal = std::make_shared<AnalogTimeSeries>(std::move(constant_data), constant_data.size());
            m_data_manager->setData<AnalogTimeSeries>("ConstantSignal", constant_signal, TimeKey("signal_time"));
        }

        // Create Noisy Signal: random-like values but deterministic for testing
        {
            std::vector<float> noisy_data(51);
            std::generate(noisy_data.begin(), noisy_data.end(), [i = 0]() mutable {
                return static_cast<float>((i * 37 + 23) % 100) / 10.0f;// Values 0.0 to 9.9
            });
            auto noisy_signal = std::make_shared<AnalogTimeSeries>(std::move(noisy_data), noisy_data.size());
            m_data_manager->setData<AnalogTimeSeries>("NoisySignal", noisy_signal, TimeKey("signal_time"));
        }
    }

    /**
     * @brief Create behavior intervals (for testing interval reductions)
     */
    void createBehaviorIntervals() {
        // Create behavior periods for testing
        auto behavior_intervals = std::make_shared<DigitalIntervalSeries>();

        // Period 1: time 10-20 (corresponds to signal indices 5-10)
        behavior_intervals->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));

        // Period 2: time 30-40 (corresponds to signal indices 15-20)
        behavior_intervals->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));

        // Period 3: time 60-80 (corresponds to signal indices 30-40)
        behavior_intervals->addEvent(TimeFrameIndex(60), TimeFrameIndex(80));

        // Period 4: time 90-100 (corresponds to signal indices 45-50)
        behavior_intervals->addEvent(TimeFrameIndex(90), TimeFrameIndex(100));

        m_data_manager->setData<DigitalIntervalSeries>("BehaviorPeriods", behavior_intervals, TimeKey("behavior_time"));
    }
};

/**
 * @brief Test fixture combining IntervalReductionTestFixture with TableRegistry and TablePipeline
 */
class IntervalReductionTableRegistryTestFixture : public IntervalReductionTestFixture {
protected:
    IntervalReductionTableRegistryTestFixture()
        : IntervalReductionTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~IntervalReductionTableRegistryTestFixture() = default;

    /**
     * @brief Get the TableRegistry instance
     */
    TableRegistry & getTableRegistry() { return *m_table_registry_ptr; }
    TableRegistry const & getTableRegistry() const { return *m_table_registry_ptr; }
    TableRegistry * getTableRegistryPtr() { return m_table_registry_ptr; }

    /**
     * @brief Get the TablePipeline instance
     */
    TablePipeline & getTablePipeline() { return *m_table_pipeline; }
    TablePipeline const & getTablePipeline() const { return *m_table_pipeline; }
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
                        TimeFrame const * target_timeFrame) -> std::vector<float> {
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

TEST_CASE("DM - TV - IntervalReductionComputer Basic Functionality", "[IntervalReductionComputer]") {

    SECTION("Mean reduction - basic calculation") {
        // Create time frame
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Create analog data
        std::vector<float> analogData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Create intervals
        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),// values: 1, 2, 3 -> mean = 2.0
                TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5)),// values: 4, 5, 6 -> mean = 5.0
                TimeFrameInterval(TimeFrameIndex(6), TimeFrameIndex(9)) // values: 7, 8, 9, 10 -> mean = 8.5
        };

        ExecutionPlan plan(intervals, timeFrame);

        // Create the computer
        IntervalReductionComputer computer(analogSource, ReductionType::Mean);

        // Compute the results
        auto [results, entity_ids] = computer.compute(plan);

        // Verify results
        REQUIRE(results.size() == 3);
        REQUIRE(results[0] == Catch::Approx(2.0).epsilon(0.001));
        REQUIRE(results[1] == Catch::Approx(5.0).epsilon(0.001));
        REQUIRE(results[2] == Catch::Approx(8.5).epsilon(0.001));
    }

    SECTION("Max reduction - basic calculation") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 5.0f, 2.0f, 8.0f, 3.0f, 6.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),// values: 1, 5, 2 -> max = 5.0
                TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5)) // values: 8, 3, 6 -> max = 8.0
        };

        ExecutionPlan plan(intervals, timeFrame);
        IntervalReductionComputer computer(analogSource, ReductionType::Max);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 2);
        REQUIRE(results[0] == Catch::Approx(5.0).epsilon(0.001));
        REQUIRE(results[1] == Catch::Approx(8.0).epsilon(0.001));
    }

    SECTION("Min reduction - basic calculation") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 5.0f, 2.0f, 8.0f, 3.0f, 6.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),// values: 1, 5, 2 -> min = 1.0
                TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5)) // values: 8, 3, 6 -> min = 3.0
        };

        ExecutionPlan plan(intervals, timeFrame);
        IntervalReductionComputer computer(analogSource, ReductionType::Min);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 2);
        REQUIRE(results[0] == Catch::Approx(1.0).epsilon(0.001));
        REQUIRE(results[1] == Catch::Approx(3.0).epsilon(0.001));
    }

    SECTION("Sum reduction - basic calculation") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),// values: 1, 2, 3 -> sum = 6.0
                TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5)) // values: 4, 5, 6 -> sum = 15.0
        };

        ExecutionPlan plan(intervals, timeFrame);
        IntervalReductionComputer computer(analogSource, ReductionType::Sum);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 2);
        REQUIRE(results[0] == Catch::Approx(6.0).epsilon(0.001));
        REQUIRE(results[1] == Catch::Approx(15.0).epsilon(0.001));
    }

    SECTION("Count reduction - basic calculation") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),// 3 values -> count = 3.0
                TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(4)) // 2 values -> count = 2.0
        };

        ExecutionPlan plan(intervals, timeFrame);
        IntervalReductionComputer computer(analogSource, ReductionType::Count);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 2);
        REQUIRE(results[0] == Catch::Approx(3.0).epsilon(0.001));
        REQUIRE(results[1] == Catch::Approx(2.0).epsilon(0.001));
    }

    SECTION("Standard deviation reduction - basic calculation") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4, 5};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        // Use values where std dev is easy to calculate
        std::vector<float> analogData = {1.0f, 3.0f, 5.0f, 2.0f, 4.0f, 6.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2))// values: 1, 3, 5 -> mean=3, std dev = sqrt(4) = 2.0
        };

        ExecutionPlan plan(intervals, timeFrame);
        IntervalReductionComputer computer(analogSource, ReductionType::StdDev);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == Catch::Approx(2.0).epsilon(0.001));
    }

    /*
    
    SECTION("Empty interval handling") {
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        
        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);
        
        // Create an interval that results in empty data
        std::vector<TimeFrameInterval> intervals = {
            TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(6)) // Out of range -> empty
        };
        
        ExecutionPlan plan(intervals, timeFrame);
        IntervalReductionComputer computer(analogSource, ReductionType::Mean);
        
        auto [results, entity_ids] = computer.compute(plan);
        
        REQUIRE(results.size() == 1);
        std::cout << "Empty interval result: " << results[0] << std::endl;
        REQUIRE(std::isnan(results[0])); // Should return NaN for empty intervals
    }

    */

    SECTION("Single value interval") {
        std::vector<int> timeValues = {0, 1, 2, 3, 4};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        std::vector<TimeFrameInterval> intervals = {
                TimeFrameInterval(TimeFrameIndex(2), TimeFrameIndex(2))// Single value: 3.0
        };

        ExecutionPlan plan(intervals, timeFrame);

        // Test different reductions on single value
        IntervalReductionComputer meanComputer(analogSource, ReductionType::Mean);
        IntervalReductionComputer stdDevComputer(analogSource, ReductionType::StdDev);

        auto [meanResults, meanEntity_ids] = meanComputer.compute(plan);
        auto [stdDevResults, stdDevEntity_ids] = stdDevComputer.compute(plan);

        REQUIRE(meanResults.size() == 1);
        REQUIRE(stdDevResults.size() == 1);
        REQUIRE(meanResults[0] == Catch::Approx(3.0).epsilon(0.001));
        REQUIRE(stdDevResults[0] == Catch::Approx(0.0).epsilon(0.001));// Std dev of single value is 0
    }
}

TEST_CASE("DM - TV - IntervalReductionComputer Error Handling", "[IntervalReductionComputer][Error]") {

    SECTION("Null source throws exception") {
        REQUIRE_THROWS_AS(IntervalReductionComputer(nullptr, ReductionType::Mean),
                          std::invalid_argument);
    }

    SECTION("ExecutionPlan without intervals throws exception") {
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Create execution plan with indices instead of intervals
        std::vector<TimeFrameIndex> indices = {TimeFrameIndex(0), TimeFrameIndex(1)};
        ExecutionPlan plan(indices, timeFrame);

        IntervalReductionComputer computer(analogSource, ReductionType::Mean);

        REQUIRE_THROWS_AS(computer.compute(plan), std::invalid_argument);
    }
}

TEST_CASE("DM - TV - IntervalReductionComputer Dependency Tracking", "[IntervalReductionComputer][Dependencies]") {

    SECTION("getSourceDependency returns correct source name") {
        std::vector<int> timeValues = {0, 1, 2};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> analogData = {1.0f, 2.0f, 3.0f};
        auto analogSource = std::make_shared<MockAnalogSource>("TestSignal", timeFrame, analogData);

        // Test default source name (from source)
        IntervalReductionComputer computer1(analogSource, ReductionType::Mean);
        REQUIRE(computer1.getSourceDependency() == "TestSignal");

        // Test custom source name
        IntervalReductionComputer computer2(analogSource, ReductionType::Mean, "CustomName");
        REQUIRE(computer2.getSourceDependency() == "CustomName");
    }
}

TEST_CASE_METHOD(IntervalReductionTestFixture, "DM - TV - IntervalReductionComputer with DataManager fixture", "[IntervalReductionComputer][DataManager][Fixture]") {

    SECTION("Test with linear signal data from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the linear signal source from the DataManager
        auto linear_source = dme->getAnalogSource("LinearSignal");
        REQUIRE(linear_source != nullptr);

        // Create interval selector from behavior intervals
        auto behavior_time_frame = dm.getTime(TimeKey("behavior_time"));
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        REQUIRE(behavior_source != nullptr);

        auto behavior_intervals = behavior_source->getIntervalsInRange(
                TimeFrameIndex(0),
                TimeFrameIndex(100),
                *behavior_time_frame);

        // Convert to TimeFrameIntervals for row selector
        std::vector<TimeFrameInterval> row_intervals;
        for (auto const & interval: behavior_intervals) {
            row_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }

        REQUIRE(row_intervals.size() == 4);

        auto row_selector = std::make_unique<IntervalSelector>(row_intervals, behavior_time_frame);

        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Test different reduction operations
        builder.addColumn<double>("LinearMean",
                                  std::make_unique<IntervalReductionComputer>(linear_source, ReductionType::Mean));
        builder.addColumn<double>("LinearMax",
                                  std::make_unique<IntervalReductionComputer>(linear_source, ReductionType::Max));
        builder.addColumn<double>("LinearMin",
                                  std::make_unique<IntervalReductionComputer>(linear_source, ReductionType::Min));
        builder.addColumn<double>("LinearSum",
                                  std::make_unique<IntervalReductionComputer>(linear_source, ReductionType::Sum));

        // Build the table
        TableView table = builder.build();

        // Verify table structure
        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 4);
        REQUIRE(table.hasColumn("LinearMean"));
        REQUIRE(table.hasColumn("LinearMax"));
        REQUIRE(table.hasColumn("LinearMin"));
        REQUIRE(table.hasColumn("LinearSum"));

        // Get the column data
        auto means = table.getColumnValues<double>("LinearMean");
        auto maxes = table.getColumnValues<double>("LinearMax");
        auto mins = table.getColumnValues<double>("LinearMin");
        auto sums = table.getColumnValues<double>("LinearSum");

        REQUIRE(means.size() == 4);
        REQUIRE(maxes.size() == 4);
        REQUIRE(mins.size() == 4);
        REQUIRE(sums.size() == 4);

        // Verify that all results are reasonable
        for (size_t i = 0; i < 4; ++i) {
            REQUIRE(means[i] > 0.0);      // Linear signal is always positive
            REQUIRE(maxes[i] >= means[i]);// Max should be >= mean
            REQUIRE(mins[i] <= means[i]); // Min should be <= mean
            REQUIRE(sums[i] > 0.0);       // Sum should be positive

            std::cout << "Interval " << i << ": Mean=" << means[i]
                      << ", Max=" << maxes[i] << ", Min=" << mins[i]
                      << ", Sum=" << sums[i] << std::endl;
        }
    }

    SECTION("Test with constant signal - verify expected values") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        auto constant_source = dme->getAnalogSource("ConstantSignal");
        REQUIRE(constant_source != nullptr);

        // Create a simple test interval
        auto signal_time_frame = dm.getTime(TimeKey("signal_time"));

        std::vector<TimeFrameInterval> test_intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(10))// Signal indices 0-10
        };

        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, signal_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add reduction columns
        builder.addColumn<double>("ConstantMean",
                                  std::make_unique<IntervalReductionComputer>(constant_source, ReductionType::Mean));
        builder.addColumn<double>("ConstantStdDev",
                                  std::make_unique<IntervalReductionComputer>(constant_source, ReductionType::StdDev));
        builder.addColumn<double>("ConstantCount",
                                  std::make_unique<IntervalReductionComputer>(constant_source, ReductionType::Count));

        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 3);

        auto means = table.getColumnValues<double>("ConstantMean");
        auto stdDevs = table.getColumnValues<double>("ConstantStdDev");
        auto counts = table.getColumnValues<double>("ConstantCount");

        REQUIRE(means.size() == 1);
        REQUIRE(stdDevs.size() == 1);
        REQUIRE(counts.size() == 1);

        // Constant signal should have mean = 5.0, std dev = 0.0
        REQUIRE(means[0] == Catch::Approx(5.0).epsilon(0.001));
        REQUIRE(stdDevs[0] == Catch::Approx(0.0).epsilon(0.001));
        REQUIRE(counts[0] == Catch::Approx(11.0).epsilon(0.001));// 11 samples in range 0-10

        std::cout << "Constant signal - Mean: " << means[0]
                  << ", StdDev: " << stdDevs[0] << ", Count: " << counts[0] << std::endl;
    }
}

TEST_CASE_METHOD(IntervalReductionTableRegistryTestFixture, "DM - TV - IntervalReductionComputer via ComputerRegistry", "[IntervalReductionComputer][Registry]") {

    SECTION("Verify IntervalReductionComputer is registered in ComputerRegistry") {
        auto & registry = getTableRegistry().getComputerRegistry();

        // Check that all interval reduction computers are registered
        auto mean_info = registry.findComputerInfo("Interval Mean");
        auto max_info = registry.findComputerInfo("Interval Max");
        auto min_info = registry.findComputerInfo("Interval Min");
        auto stddev_info = registry.findComputerInfo("Interval Standard Deviation");
        auto sum_info = registry.findComputerInfo("Interval Sum");
        auto count_info = registry.findComputerInfo("Interval Count");

        REQUIRE(mean_info != nullptr);
        REQUIRE(max_info != nullptr);
        REQUIRE(min_info != nullptr);
        REQUIRE(stddev_info != nullptr);
        REQUIRE(sum_info != nullptr);
        REQUIRE(count_info != nullptr);

        // Verify computer info details for mean
        REQUIRE(mean_info->name == "Interval Mean");
        REQUIRE(mean_info->outputType == typeid(double));
        REQUIRE(mean_info->outputTypeName == "double");
        REQUIRE(mean_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(mean_info->requiredSourceType == typeid(std::shared_ptr<IAnalogSource>));

        // Verify other computers have correct output types
        REQUIRE(max_info->outputType == typeid(double));
        REQUIRE(min_info->outputType == typeid(double));
        REQUIRE(stddev_info->outputType == typeid(double));
        REQUIRE(sum_info->outputType == typeid(double));
        REQUIRE(count_info->outputType == typeid(double));
    }

    SECTION("Create IntervalReductionComputer via ComputerRegistry") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        // Get linear signal source for testing
        auto linear_source = dme->getAnalogSource("LinearSignal");
        REQUIRE(linear_source != nullptr);

        // Create computers via registry
        std::map<std::string, std::string> empty_params;

        auto mean_computer = registry.createTypedComputer<double>(
                "Interval Mean", linear_source, empty_params);
        auto max_computer = registry.createTypedComputer<double>(
                "Interval Max", linear_source, empty_params);
        auto sum_computer = registry.createTypedComputer<double>(
                "Interval Sum", linear_source, empty_params);

        REQUIRE(mean_computer != nullptr);
        REQUIRE(max_computer != nullptr);
        REQUIRE(sum_computer != nullptr);

        // Test that the created computers work correctly
        auto signal_time_frame = dm.getTime(TimeKey("signal_time"));

        // Create a simple test interval
        std::vector<TimeFrameInterval> test_intervals = {
                TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(4))// Signal indices 0-4 -> values 1,2,3,4,5
        };

        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, signal_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Use the registry-created computers
        builder.addColumn("RegistryMean", std::move(mean_computer));
        builder.addColumn("RegistryMax", std::move(max_computer));
        builder.addColumn("RegistrySum", std::move(sum_computer));

        // Build and verify the table
        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 1);
        REQUIRE(table.getColumnCount() == 3);
        REQUIRE(table.hasColumn("RegistryMean"));
        REQUIRE(table.hasColumn("RegistryMax"));
        REQUIRE(table.hasColumn("RegistrySum"));

        auto means = table.getColumnValues<double>("RegistryMean");
        auto maxes = table.getColumnValues<double>("RegistryMax");
        auto sums = table.getColumnValues<double>("RegistrySum");

        REQUIRE(means.size() == 1);
        REQUIRE(maxes.size() == 1);
        REQUIRE(sums.size() == 1);

        // Verify reasonable results (values 1,2,3,4,5)
        REQUIRE(means[0] == Catch::Approx(3.0).epsilon(0.001));// Mean of 1,2,3,4,5 = 3
        REQUIRE(maxes[0] == Catch::Approx(5.0).epsilon(0.001));// Max of 1,2,3,4,5 = 5
        REQUIRE(sums[0] == Catch::Approx(15.0).epsilon(0.001));// Sum of 1,2,3,4,5 = 15

        std::cout << "Registry test - Mean: " << means[0]
                  << ", Max: " << maxes[0] << ", Sum: " << sums[0] << std::endl;
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
                "Interval Mean", linear_source, empty_params);

        // Create computer directly
        auto direct_computer = std::make_unique<IntervalReductionComputer>(
                linear_source, ReductionType::Mean);

        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);

        // Test both computers with the same data
        auto signal_time_frame = dm.getTime(TimeKey("signal_time"));
        std::vector<TimeFrameInterval> test_intervals = {
                TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(9))};

        ExecutionPlan plan(test_intervals, signal_time_frame);

        auto [registry_result, registry_entity_ids] = registry_computer->compute(plan);
        auto [direct_result, direct_entity_ids] = direct_computer->compute(plan);

        REQUIRE(registry_result.size() == 1);
        REQUIRE(direct_result.size() == 1);

        // Results should be identical
        REQUIRE(registry_result[0] == Catch::Approx(direct_result[0]).epsilon(0.001));

        std::cout << "Comparison test - Registry result: " << registry_result[0]
                  << ", Direct result: " << direct_result[0] << std::endl;
    }
}

TEST_CASE_METHOD(IntervalReductionTableRegistryTestFixture, "DM - TV - IntervalReductionComputer via JSON TablePipeline", "[IntervalReductionComputer][JSON][Pipeline]") {

    SECTION("Test multiple reduction operations via JSON pipeline") {
        // JSON configuration for testing IntervalReductionComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Interval Reduction Test",
                "description": "Test JSON execution of IntervalReductionComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "interval_reduction_test",
                    "name": "Interval Reduction Test Table",
                    "description": "Test table using IntervalReductionComputer",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "LinearSignalMean",
                            "description": "Mean of linear signal over each behavior period",
                            "data_source": "LinearSignal",
                            "computer": "Interval Mean"
                        },
                        {
                            "name": "LinearSignalMax",
                            "description": "Maximum of linear signal over each behavior period",
                            "data_source": "LinearSignal",
                            "computer": "Interval Max"
                        },
                        {
                            "name": "LinearSignalMin",
                            "description": "Minimum of linear signal over each behavior period",
                            "data_source": "LinearSignal",
                            "computer": "Interval Min"
                        },
                        {
                            "name": "LinearSignalSum",
                            "description": "Sum of linear signal over each behavior period",
                            "data_source": "LinearSignal",
                            "computer": "Interval Sum"
                        },
                        {
                            "name": "LinearSignalStdDev",
                            "description": "Standard deviation of linear signal over each behavior period",
                            "data_source": "LinearSignal",
                            "computer": "Interval Standard Deviation"
                        },
                        {
                            "name": "LinearSignalCount",
                            "description": "Count of linear signal samples over each behavior period",
                            "data_source": "LinearSignal",
                            "computer": "Interval Count"
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
        REQUIRE(config.table_id == "interval_reduction_test");
        REQUIRE(config.name == "Interval Reduction Test Table");
        REQUIRE(config.columns.size() == 6);

        // Verify column configurations
        REQUIRE(config.columns[0]["name"] == "LinearSignalMean");
        REQUIRE(config.columns[0]["computer"] == "Interval Mean");
        REQUIRE(config.columns[1]["computer"] == "Interval Max");
        REQUIRE(config.columns[2]["computer"] == "Interval Min");
        REQUIRE(config.columns[3]["computer"] == "Interval Sum");
        REQUIRE(config.columns[4]["computer"] == "Interval Standard Deviation");
        REQUIRE(config.columns[5]["computer"] == "Interval Count");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "interval");
        REQUIRE(config.row_selector["source"] == "BehaviorPeriods");

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
            REQUIRE(registry.hasTable("interval_reduction_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("interval_reduction_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const & name: column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 6);
            REQUIRE(built_table->hasColumn("LinearSignalMean"));
            REQUIRE(built_table->hasColumn("LinearSignalMax"));
            REQUIRE(built_table->hasColumn("LinearSignalMin"));
            REQUIRE(built_table->hasColumn("LinearSignalSum"));
            REQUIRE(built_table->hasColumn("LinearSignalStdDev"));
            REQUIRE(built_table->hasColumn("LinearSignalCount"));

            // Verify table has 4 rows (one for each behavior period)
            REQUIRE(built_table->getRowCount() == 4);

            // Get and verify the computed values
            auto means = built_table->getColumnValues<double>("LinearSignalMean");
            auto maxes = built_table->getColumnValues<double>("LinearSignalMax");
            auto mins = built_table->getColumnValues<double>("LinearSignalMin");
            auto sums = built_table->getColumnValues<double>("LinearSignalSum");
            auto stddevs = built_table->getColumnValues<double>("LinearSignalStdDev");
            auto counts = built_table->getColumnValues<double>("LinearSignalCount");

            REQUIRE(means.size() == 4);
            REQUIRE(maxes.size() == 4);
            REQUIRE(mins.size() == 4);
            REQUIRE(sums.size() == 4);
            REQUIRE(stddevs.size() == 4);
            REQUIRE(counts.size() == 4);

            for (size_t i = 0; i < 4; ++i) {
                // Verify mathematical relationships
                REQUIRE(maxes[i] >= means[i]);// Max >= Mean
                REQUIRE(means[i] >= mins[i]); // Mean >= Min
                REQUIRE(sums[i] > 0.0);       // Sum should be positive (linear signal is positive)
                REQUIRE(stddevs[i] >= 0.0);   // Standard deviation is non-negative
                REQUIRE(counts[i] > 0.0);     // Count should be positive

                // Verify mathematical consistency: sum = mean * count (approximately)
                REQUIRE(sums[i] == Catch::Approx(means[i] * counts[i]).epsilon(0.001));

                std::cout << "Row " << i << ": Mean=" << means[i]
                          << ", Max=" << maxes[i] << ", Min=" << mins[i]
                          << ", Sum=" << sums[i] << ", StdDev=" << stddevs[i]
                          << ", Count=" << counts[i] << std::endl;
            }

        } else {
            std::cout << "Pipeline execution failed: " << pipeline_result.error_message << std::endl;
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test with different signal types via JSON") {
        char const * json_config = R"({
            "metadata": {
                "name": "Multi-Signal Reduction Test",
                "description": "Test reduction operations on different signal types"
            },
            "tables": [
                {
                    "table_id": "multi_signal_reduction_test",
                    "name": "Multi Signal Reduction Test Table",
                    "description": "Test table using multiple analog signals",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "LinearMean",
                            "description": "Mean of linear signal",
                            "data_source": "LinearSignal",
                            "computer": "Interval Mean"
                        },
                        {
                            "name": "SineMean",
                            "description": "Mean of sine signal",
                            "data_source": "SineSignal",
                            "computer": "Interval Mean"
                        },
                        {
                            "name": "ConstantMean",
                            "description": "Mean of constant signal",
                            "data_source": "ConstantSignal",
                            "computer": "Interval Mean"
                        },
                        {
                            "name": "NoisyStdDev",
                            "description": "Standard deviation of noisy signal",
                            "data_source": "NoisySignal",
                            "computer": "Interval Standard Deviation"
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
            std::cout << "✓ Multi-signal reduction pipeline executed successfully!" << std::endl;

            auto & registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("multi_signal_reduction_test");
            REQUIRE(built_table != nullptr);

            REQUIRE(built_table->getRowCount() == 4);
            REQUIRE(built_table->getColumnCount() == 4);

            // Verify all expected columns exist
            REQUIRE(built_table->hasColumn("LinearMean"));
            REQUIRE(built_table->hasColumn("SineMean"));
            REQUIRE(built_table->hasColumn("ConstantMean"));
            REQUIRE(built_table->hasColumn("NoisyStdDev"));

            // Get column data
            auto linear_means = built_table->getColumnValues<double>("LinearMean");
            auto sine_means = built_table->getColumnValues<double>("SineMean");
            auto constant_means = built_table->getColumnValues<double>("ConstantMean");
            auto noisy_stddevs = built_table->getColumnValues<double>("NoisyStdDev");

            // Verify data characteristics
            for (size_t i = 0; i < 4; ++i) {
                REQUIRE(linear_means[i] > 0.0);                                 // Linear signal is always positive
                REQUIRE(constant_means[i] == Catch::Approx(5.0).epsilon(0.001));// Constant signal = 5.0
                REQUIRE(noisy_stddevs[i] >= 0.0);                               // Std dev is non-negative
                // Sine mean could be positive, negative, or close to zero depending on interval

                std::cout << "Row " << i << ": Linear=" << linear_means[i]
                          << ", Sine=" << sine_means[i]
                          << ", Constant=" << constant_means[i]
                          << ", NoisyStdDev=" << noisy_stddevs[i] << std::endl;
            }

        } else {
            FAIL("Multi-signal pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}
