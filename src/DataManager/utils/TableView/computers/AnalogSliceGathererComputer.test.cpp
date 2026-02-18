#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogSliceGathererComputer.h"
#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IAnalogSource.h"
#include "TimeFrame/TimeFrame.hpp"

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

#include "fixtures/scenarios/analog/analog_slice_gatherer_scenarios.hpp"

#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

// ---------------------------------------------------------------------------
// Minimal IAnalogSource mock – wraps an AnalogTimeSeries + TimeFrame
// ---------------------------------------------------------------------------
class MockAnalogSource : public IAnalogSource {
public:
    MockAnalogSource(std::string name,
                     std::shared_ptr<TimeFrame> timeFrame,
                     std::vector<float> data)
        : m_name(std::move(name)),
          m_timeFrame(std::move(timeFrame)),
          m_data(std::move(data)) {}

    [[nodiscard]] auto getName() const -> std::string const & override { return m_name; }
    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override { return m_timeFrame; }
    [[nodiscard]] auto size() const -> size_t override { return m_data.size(); }

    auto getDataInRange(TimeFrameIndex start, TimeFrameIndex end,
                        TimeFrame const * target_timeFrame) -> std::vector<float> override {
        auto startTime = target_timeFrame->getTimeAtIndex(start);
        auto endTime   = target_timeFrame->getTimeAtIndex(end);

        auto startIndex = m_timeFrame->getIndexAtTime(startTime);
        auto endIndex   = m_timeFrame->getIndexAtTime(endTime);

        size_t startIdx = std::max(0L, static_cast<long>(startIndex.getValue()));
        size_t endIdx   = std::min(static_cast<long>(m_data.size() - 1),
                                   static_cast<long>(endIndex.getValue()));

        if (startIdx > endIdx || startIdx >= m_data.size()) {
            return {};
        }
        return {m_data.data() + startIdx, m_data.data() + endIdx + 1};
    }

private:
    std::string                m_name;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::vector<float>         m_data;
};

// ---------------------------------------------------------------------------
// Helper: build a MockAnalogSource from an AnalogTimeSeries + TimeFrame
// ---------------------------------------------------------------------------
static std::shared_ptr<MockAnalogSource>
makeMock(std::string name,
         std::shared_ptr<TimeFrame> tf,
         std::shared_ptr<AnalogTimeSeries> ats)
{
    auto vals = ats->getAnalogTimeSeries();
    std::vector<float> data(vals.begin(), vals.end());
    return std::make_shared<MockAnalogSource>(std::move(name), std::move(tf), std::move(data));
}

// ---------------------------------------------------------------------------
// Fixture: DataManager populated from the cross-timeframe scenario
// ---------------------------------------------------------------------------
class AnalogSliceGathererTestFixture {
protected:
    AnalogSliceGathererTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        auto d = analog_slice_gatherer_scenarios::cross_timeframe_analog_with_behavior();

        m_data_manager->setTime(TimeKey("analog_time"),   d.analog_timeframe,   true);
        m_data_manager->setTime(TimeKey("behavior_time"), d.behavior_timeframe, true);

        m_data_manager->setData<AnalogTimeSeries>(
            "TriangularWave", d.triangular_wave, TimeKey("analog_time"));
        m_data_manager->setData<AnalogTimeSeries>(
            "SineWave", d.sine_wave, TimeKey("analog_time"));
        m_data_manager->setData<DigitalIntervalSeries>(
            "BehaviorPeriods", d.behavior_periods, TimeKey("behavior_time"));
    }

    DataManager &       getDataManager()       { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }

private:
    std::unique_ptr<DataManager> m_data_manager;
};

// ---------------------------------------------------------------------------
// Extended fixture that also exposes TableRegistry + TablePipeline
// ---------------------------------------------------------------------------
class AnalogSliceTableRegistryTestFixture : public AnalogSliceGathererTestFixture {
protected:
    AnalogSliceTableRegistryTestFixture()
        : AnalogSliceGathererTestFixture()
    {
        m_table_registry_ptr = getDataManager().getTableRegistry();
        m_table_pipeline     = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    TableRegistry &   getTableRegistry()    { return *m_table_registry_ptr; }
    TablePipeline &   getTablePipeline()    { return *m_table_pipeline; }
    TableRegistry *   getTableRegistryPtr() { return m_table_registry_ptr; }

    std::shared_ptr<DataManagerExtension> getDataManagerExtension() {
        if (!m_dme) {
            m_dme = std::make_shared<DataManagerExtension>(getDataManager());
        }
        return m_dme;
    }

private:
    TableRegistry *                       m_table_registry_ptr{nullptr};
    std::unique_ptr<TablePipeline>        m_table_pipeline;
    std::shared_ptr<DataManagerExtension> m_dme;
};

// ===========================================================================
// Tests
// ===========================================================================

TEST_CASE("DM - TV - AnalogSliceGathererComputer Basic Functionality",
          "[AnalogSliceGathererComputer]") {

    SECTION("Linear ramp – three double slices") {
        auto [ats, tf, intervals] = analog_slice_gatherer_scenarios::linear_ramp_three_slices();
        auto source = makeMock("TestAnalog", tf, ats);

        ExecutionPlan plan(intervals, tf);
        AnalogSliceGathererComputer<std::vector<double>> computer(source);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 3);

        // [1,3] -> {1, 2, 3}
        REQUIRE(results[0].size() == 3);
        REQUIRE(results[0][0] == Catch::Approx(1.0));
        REQUIRE(results[0][1] == Catch::Approx(2.0));
        REQUIRE(results[0][2] == Catch::Approx(3.0));

        // [5,7] -> {5, 6, 7}
        REQUIRE(results[1].size() == 3);
        REQUIRE(results[1][0] == Catch::Approx(5.0));
        REQUIRE(results[1][1] == Catch::Approx(6.0));
        REQUIRE(results[1][2] == Catch::Approx(7.0));

        // [8,9] -> {8, 9}
        REQUIRE(results[2].size() == 2);
        REQUIRE(results[2][0] == Catch::Approx(8.0));
        REQUIRE(results[2][1] == Catch::Approx(9.0));
    }

    SECTION("Sine wave – two float slices") {
        auto [ats, tf, intervals] = analog_slice_gatherer_scenarios::sine_wave_two_slices();
        auto source = makeMock("SineWave", tf, ats);

        ExecutionPlan plan(intervals, tf);
        AnalogSliceGathererComputer<std::vector<float>> computer(source);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 2);
        REQUIRE(results[0].size() == 3);
        REQUIRE(results[1].size() == 3);

        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(results[0][i] == Catch::Approx(std::sin(static_cast<float>(i) * 0.5f)));
        }
        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(results[1][i] == Catch::Approx(std::sin(static_cast<float>(i + 3) * 0.5f)));
        }
    }

    SECTION("Single-point intervals") {
        auto [ats, tf, intervals] = analog_slice_gatherer_scenarios::single_point_intervals();
        auto source = makeMock("TestAnalog", tf, ats);

        ExecutionPlan plan(intervals, tf);
        AnalogSliceGathererComputer<std::vector<double>> computer(source);

        auto [results, entity_ids] = computer.compute(plan);

        REQUIRE(results.size() == 3);
        REQUIRE(results[0].size() == 1);
        REQUIRE(results[0][0] == Catch::Approx(1.0));
        REQUIRE(results[1].size() == 1);
        REQUIRE(results[1][0] == Catch::Approx(3.0));
        REQUIRE(results[2].size() == 1);
        REQUIRE(results[2][0] == Catch::Approx(5.0));
    }
}

TEST_CASE("DM - TV - AnalogSliceGathererComputer Error Handling",
          "[AnalogSliceGathererComputer][Error]") {

    SECTION("ExecutionPlan with indices (not intervals) throws") {
        auto [ats, tf] = analog_slice_gatherer_scenarios::simple_signal_for_error_test();
        auto source = makeMock("TestAnalog", tf, ats);

        std::vector<TimeFrameIndex> indices = {TimeFrameIndex(0), TimeFrameIndex(1)};
        ExecutionPlan plan(indices, tf);

        AnalogSliceGathererComputer<std::vector<double>> computer(source);
        REQUIRE_THROWS_AS(computer.compute(plan), std::invalid_argument);
    }
}

TEST_CASE("DM - TV - AnalogSliceGathererComputer Template Types",
          "[AnalogSliceGathererComputer][Templates]") {

    SECTION("double and float templates produce identical values") {
        auto [ats, tf, intervals] = analog_slice_gatherer_scenarios::fractional_ramp_single_slice();
        auto source = makeMock("TestAnalog", tf, ats);

        ExecutionPlan plan(intervals, tf);

        AnalogSliceGathererComputer<std::vector<double>> doubleComputer(source);
        auto [doubleResults, doubleEntity_ids] = doubleComputer.compute(plan);

        REQUIRE(doubleResults.size() == 1);
        REQUIRE(doubleResults[0].size() == 3);
        REQUIRE(doubleResults[0][0] == Catch::Approx(2.5));
        REQUIRE(doubleResults[0][1] == Catch::Approx(3.5));
        REQUIRE(doubleResults[0][2] == Catch::Approx(4.5));

        AnalogSliceGathererComputer<std::vector<float>> floatComputer(source);
        auto [floatResults, floatEntity_ids] = floatComputer.compute(plan);

        REQUIRE(floatResults.size() == 1);
        REQUIRE(floatResults[0].size() == 3);
        REQUIRE(floatResults[0][0] == Catch::Approx(2.5f));
        REQUIRE(floatResults[0][1] == Catch::Approx(3.5f));
        REQUIRE(floatResults[0][2] == Catch::Approx(4.5f));
    }
}

TEST_CASE("DM - TV - AnalogSliceGathererComputer Dependency Tracking",
          "[AnalogSliceGathererComputer][Dependencies]") {

    SECTION("getSourceDependency returns the source name") {
        auto [ats, tf] = analog_slice_gatherer_scenarios::minimal_signal();
        auto source = makeMock("TestSource", tf, ats);

        // Default: name taken from the source object
        AnalogSliceGathererComputer<std::vector<double>> computer1(source);
        REQUIRE(computer1.getSourceDependency() == "TestSource");

        // Explicit override name
        AnalogSliceGathererComputer<std::vector<double>> computer2(source, "CustomSourceName");
        REQUIRE(computer2.getSourceDependency() == "CustomSourceName");
    }
}

TEST_CASE_METHOD(AnalogSliceGathererTestFixture,
                 "DM - TV - AnalogSliceGathererComputer with DataManager",
                 "[AnalogSliceGathererComputer][DataManager]") {

    SECTION("Triangular wave sliced by behavior intervals") {
        auto & dm  = getDataManager();
        auto   dme = std::make_shared<DataManagerExtension>(dm);

        auto triangular_source = dme->getAnalogSource("TriangularWave");
        REQUIRE(triangular_source != nullptr);

        auto behavior_tf              = dm.getTime(TimeKey("behavior_time"));
        auto behavior_interval_source = dm.getData<DigitalIntervalSeries>("BehaviorPeriods");
        REQUIRE(behavior_interval_source != nullptr);

        auto raw = behavior_interval_source->getIntervalsInRange(
            TimeFrameIndex(0), TimeFrameIndex(50), *behavior_tf);

        std::vector<TimeFrameInterval> row_intervals;
        for (auto const & iv : raw) {
            row_intervals.emplace_back(TimeFrameIndex(iv.start), TimeFrameIndex(iv.end));
        }
        REQUIRE(row_intervals.size() == 3);

        auto row_selector = std::make_unique<IntervalSelector>(row_intervals, behavior_tf);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        builder.addColumn<std::vector<double>>(
            "TriangularSlices",
            std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(
                triangular_source, "TriangularWave"));

        TableView table = builder.build();

        REQUIRE(table.getRowCount()    == 3);
        REQUIRE(table.getColumnCount() == 1);
        REQUIRE(table.hasColumn("TriangularSlices"));

        auto slices = table.getColumnValues<std::vector<double>>("TriangularSlices");
        REQUIRE(slices.size() == 3);
        for (auto const & slice : slices) {
            REQUIRE(slice.size() > 0);
            for (auto v : slice) {
                REQUIRE(v >= 0.0);
                REQUIRE(v <= 50.0);
            }
        }
    }

    SECTION("Cross-timeframe: behavior rows mapped onto analog signal") {
        auto & dm  = getDataManager();
        auto   dme = std::make_shared<DataManagerExtension>(dm);

        auto triangular_source        = dme->getAnalogSource("TriangularWave");
        auto behavior_interval_source = dm.getData<DigitalIntervalSeries>("BehaviorPeriods");
        REQUIRE(triangular_source != nullptr);
        REQUIRE(behavior_interval_source != nullptr);

        auto analog_tf   = triangular_source->getTimeFrame();
        auto behavior_tf = behavior_interval_source->getTimeFrame();
        REQUIRE(analog_tf   != behavior_tf);
        REQUIRE(analog_tf->getTotalFrameCount()  == 101); // 0..100 step 1
        REQUIRE(behavior_tf->getTotalFrameCount() ==  51); // 0,2,4,...,100

        // Single behavior window: indices [5,15] in behavior_time = times 10..30
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(15)),
        };

        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_tf);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        builder.addColumn<std::vector<double>>(
            "TriangularSlice",
            std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(
                triangular_source, "TriangularWave"));

        TableView table = builder.build();

        REQUIRE(table.getRowCount()    == 1);
        REQUIRE(table.getColumnCount() == 1);

        auto slices = table.getColumnValues<std::vector<double>>("TriangularSlice");
        REQUIRE(slices.size() == 1);
        REQUIRE(slices[0].size() > 0);

        // Behavior window covers the rising edge (analog times 10–30), values 10..30
        for (auto v : slices[0]) {
            REQUIRE(v >= 10.0);
            REQUIRE(v <= 30.0);
        }
    }
}

TEST_CASE_METHOD(AnalogSliceTableRegistryTestFixture,
                 "DM - TV - AnalogSliceGathererComputer via ComputerRegistry",
                 "[AnalogSliceGathererComputer][Registry]") {

    SECTION("double and float computers are registered") {
        auto & registry = getTableRegistry().getComputerRegistry();

        auto double_info = registry.findComputerInfo("Analog Slice Gatherer");
        auto float_info  = registry.findComputerInfo("Analog Slice Gatherer Float");

        REQUIRE(double_info != nullptr);
        REQUIRE(float_info  != nullptr);

        REQUIRE(double_info->name              == "Analog Slice Gatherer");
        REQUIRE(double_info->outputType        == typeid(std::vector<double>));
        REQUIRE(double_info->outputTypeName    == "std::vector<double>");
        REQUIRE(double_info->isVectorType      == true);
        REQUIRE(double_info->elementType       == typeid(double));
        REQUIRE(double_info->elementTypeName   == "double");
        REQUIRE(double_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(double_info->requiredSourceType  == typeid(std::shared_ptr<IAnalogSource>));

        REQUIRE(float_info->name              == "Analog Slice Gatherer Float");
        REQUIRE(float_info->outputType        == typeid(std::vector<float>));
        REQUIRE(float_info->outputTypeName    == "std::vector<float>");
        REQUIRE(float_info->isVectorType      == true);
        REQUIRE(float_info->elementType       == typeid(float));
        REQUIRE(float_info->elementTypeName   == "float");
        REQUIRE(float_info->requiredRowSelector == RowSelectorType::IntervalBased);
        REQUIRE(float_info->requiredSourceType  == typeid(std::shared_ptr<IAnalogSource>));
    }

    SECTION("Registry-created computers produce same results as directly-created ones") {
        auto & dm       = getDataManager();
        auto   dme      = getDataManagerExtension();
        auto & registry = getTableRegistry().getComputerRegistry();

        auto triangular_source = dme->getAnalogSource("TriangularWave");
        REQUIRE(triangular_source != nullptr);

        std::map<std::string, std::string> empty_params;
        auto registry_computer = registry.createTypedComputer<std::vector<double>>(
            "Analog Slice Gatherer", triangular_source, empty_params);
        auto direct_computer = std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(
            triangular_source, "TriangularWave");

        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer   != nullptr);

        auto behavior_tf = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(20), TimeFrameIndex(30)),
        };
        ExecutionPlan plan(test_intervals, behavior_tf);

        auto [registry_result, reg_ids] = registry_computer->compute(plan);
        auto [direct_result,   dir_ids] = direct_computer->compute(plan);

        REQUIRE(registry_result.size() == 1);
        REQUIRE(direct_result.size()   == 1);
        REQUIRE(registry_result[0].size() == direct_result[0].size());

        for (size_t i = 0; i < registry_result[0].size(); ++i) {
            REQUIRE(registry_result[0][i] == Catch::Approx(direct_result[0][i]));
        }
    }

    SECTION("Registry-created computers integrated into TableViewBuilder") {
        auto & dm       = getDataManager();
        auto   dme      = getDataManagerExtension();
        auto & registry = getTableRegistry().getComputerRegistry();

        auto triangular_source = dme->getAnalogSource("TriangularWave");
        REQUIRE(triangular_source != nullptr);

        std::map<std::string, std::string> empty_params;
        auto double_computer = registry.createTypedComputer<std::vector<double>>(
            "Analog Slice Gatherer", triangular_source, empty_params);
        auto float_computer = registry.createTypedComputer<std::vector<float>>(
            "Analog Slice Gatherer Float", triangular_source, empty_params);

        REQUIRE(double_computer != nullptr);
        REQUIRE(float_computer  != nullptr);

        auto behavior_tf = dm.getTime(TimeKey("behavior_time"));
        std::vector<TimeFrameInterval> test_intervals = {
            TimeFrameInterval(TimeFrameIndex(20), TimeFrameIndex(30)),
        };
        auto row_selector = std::make_unique<IntervalSelector>(test_intervals, behavior_tf);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        builder.addColumn("RegistryDoubleSlice", std::move(double_computer));
        builder.addColumn("RegistryFloatSlice",  std::move(float_computer));

        TableView table = builder.build();

        REQUIRE(table.getRowCount()    == 1);
        REQUIRE(table.getColumnCount() == 2);
        REQUIRE(table.hasColumn("RegistryDoubleSlice"));
        REQUIRE(table.hasColumn("RegistryFloatSlice"));

        auto double_slices = table.getColumnValues<std::vector<double>>("RegistryDoubleSlice");
        auto float_slices  = table.getColumnValues<std::vector<float>>("RegistryFloatSlice");

        REQUIRE(double_slices.size() == 1);
        REQUIRE(float_slices.size()  == 1);
        REQUIRE(double_slices[0].size() == float_slices[0].size());

        for (auto v : double_slices[0]) {
            REQUIRE(v >= 20.0);
            REQUIRE(v <= 50.0);
        }
    }
}

TEST_CASE_METHOD(AnalogSliceTableRegistryTestFixture,
                 "DM - TV - AnalogSliceGathererComputer via JSON TablePipeline",
                 "[AnalogSliceGathererComputer][JSON][Pipeline]") {

    SECTION("double computer via JSON pipeline") {
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
                            "description": "Triangular wave slices during behavior periods",
                            "data_source": "TriangularWave",
                            "computer": "Analog Slice Gatherer"
                        },
                        {
                            "name": "SineSlices",
                            "description": "Sine wave slices during behavior periods",
                            "data_source": "SineWave",
                            "computer": "Analog Slice Gatherer"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        REQUIRE(pipeline.loadFromJson(json_obj));

        auto configs = pipeline.getTableConfigurations();
        REQUIRE(configs.size() == 1);
        REQUIRE(configs[0].table_id == "analog_slice_test");
        REQUIRE(configs[0].columns.size() == 2);
        REQUIRE(configs[0].columns[0]["computer"]    == "Analog Slice Gatherer");
        REQUIRE(configs[0].columns[0]["data_source"] == "TriangularWave");
        REQUIRE(configs[0].columns[1]["computer"]    == "Analog Slice Gatherer");
        REQUIRE(configs[0].columns[1]["data_source"] == "SineWave");
        REQUIRE(configs[0].row_selector["type"]   == "interval");
        REQUIRE(configs[0].row_selector["source"] == "BehaviorPeriods");

        auto result = pipeline.execute();
        REQUIRE(result.success);

        auto & reg = getTableRegistry();
        REQUIRE(reg.hasTable("analog_slice_test"));

        auto built = reg.getBuiltTable("analog_slice_test");
        REQUIRE(built != nullptr);
        REQUIRE(built->getRowCount()    == 3);
        REQUIRE(built->getColumnCount() == 2);
        REQUIRE(built->hasColumn("TriangularSlices"));
        REQUIRE(built->hasColumn("SineSlices"));

        auto tri  = built->getColumnValues<std::vector<double>>("TriangularSlices");
        auto sine = built->getColumnValues<std::vector<double>>("SineSlices");

        REQUIRE(tri.size()  == 3);
        REQUIRE(sine.size() == 3);

        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(tri[i].size()  > 0);
            REQUIRE(sine[i].size() > 0);
            for (auto v : tri[i]) {
                REQUIRE(v >= 0.0);
                REQUIRE(v <= 50.0);
            }
            for (auto v : sine[i]) {
                REQUIRE(v >= -25.0);
                REQUIRE(v <=  25.0);
            }
        }
    }

    SECTION("float computer via JSON pipeline") {
        char const * json_config = R"({
            "metadata": {
                "name": "Analog Slice Gatherer Float Test"
            },
            "tables": [
                {
                    "table_id": "analog_slice_float_test",
                    "name": "Analog Slice Float Test Table",
                    "row_selector": {
                        "type": "interval",
                        "source": "BehaviorPeriods"
                    },
                    "columns": [
                        {
                            "name": "TriangularSlicesFloat",
                            "data_source": "TriangularWave",
                            "computer": "Analog Slice Gatherer Float"
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        REQUIRE(pipeline.loadFromJson(json_obj));

        auto configs = pipeline.getTableConfigurations();
        REQUIRE(configs.size() == 1);
        REQUIRE(configs[0].columns[0]["computer"] == "Analog Slice Gatherer Float");

        auto result = pipeline.execute();
        REQUIRE(result.success);

        auto built = getTableRegistry().getBuiltTable("analog_slice_float_test");
        REQUIRE(built != nullptr);
        REQUIRE(built->getRowCount()    == 3);
        REQUIRE(built->getColumnCount() == 1);
        REQUIRE(built->hasColumn("TriangularSlicesFloat"));

        auto float_slices = built->getColumnValues<std::vector<float>>("TriangularSlicesFloat");
        REQUIRE(float_slices.size() == 3);
        for (auto const & slice : float_slices) {
            REQUIRE(slice.size() > 0);
            for (auto v : slice) {
                REQUIRE(v >= 0.0f);
                REQUIRE(v <= 50.0f);
            }
        }
    }
}
