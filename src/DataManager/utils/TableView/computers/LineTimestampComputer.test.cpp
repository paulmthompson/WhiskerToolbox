#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager.hpp"
#include "DataManagerTypes.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/computers/LineTimestampComputer.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/ILineSource.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <vector>

TEST_CASE("DM - TV - LineTimestampComputer basic integration", "[LineTimestampComputer]") {
    // Build a simple DataManager and inject LineData
    DataManager dm;

    // Create a TimeFrame with 3 timestamps
    std::vector<int> timeValues = {0, 1, 2};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    // Create LineData and add one simple line at each timestamp
    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);

    // simple polyline: (0,0) -> (10,0)
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {0.0f, 0.0f};
        lineData->addAtTime(TimeFrameIndex(0), xs, ys, false);
        lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);
        lineData->addAtTime(TimeFrameIndex(2), xs, ys, false);
    }

    lineData->setIdentityContext("TestLines", dm.getEntityRegistry());
    lineData->rebuildAllEntityIds();

    // Create DataManagerExtension
    DataManagerExtension dme(dm);

    // Create a TableView with Timestamp rows [0,1,2]
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    // Build LineDataAdapter directly and wrap as ILineSource
    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"TestLines"});

    // Directly construct the timestamp computer
    auto timestampComputer = std::make_unique<LineTimestampComputer>(
            std::static_pointer_cast<ILineSource>(lineAdapter),
            std::string{"TestLines"},
            tf);

    // Build the table with addColumn
    auto dme_ptr = std::make_shared<DataManagerExtension>(dme);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));
    builder.addColumn<int64_t>("Timestamp", std::move(timestampComputer));

    auto table = builder.build();

    // Expect 1 column with timestamps
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 1);
    REQUIRE(table.hasColumn("Timestamp"));

    // Validate timestamp values
    auto const & timestamps_col = table.getColumnValues<int64_t>("Timestamp");
    REQUIRE(timestamps_col.size() == 3);
    REQUIRE(timestamps_col[0] == 0);
    REQUIRE(timestamps_col[1] == 1);
    REQUIRE(timestamps_col[2] == 2);
}

TEST_CASE("DM - TV - LineTimestampComputer can be created via registry", "[LineTimestampComputer][Registry]") {
    DataManager dm;

    std::vector<int> timeValues = {0, 1};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);
    std::vector<float> xs = {0.0f, 10.0f};
    std::vector<float> ys = {0.0f, 0.0f};
    lineData->addAtTime(TimeFrameIndex(0), xs, ys, false);
    lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);

    lineData->setIdentityContext("RegLines", dm.getEntityRegistry());
    lineData->rebuildAllEntityIds();

    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"RegLines"});

    // Create DataSourceVariant via registry adapter to ensure consistent type usage
    ComputerRegistry registry;
    auto adapted = registry.createAdapter(
            "Line Data",
            std::static_pointer_cast<void>(lineData),
            tf,
            std::string{"RegLines"},
            {});

    // Fallback to direct adapter if registry adapter not found
    DataSourceVariant variant = adapted.index() != std::variant_npos ? adapted
                                                                     : DataSourceVariant{std::static_pointer_cast<ILineSource>(lineAdapter)};

    // Create via registry
    auto timestampComputer = registry.createTypedComputer<int64_t>("Line Timestamp", variant);
    REQUIRE(timestampComputer != nullptr);

    // Build with builder
    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));
    builder.addColumn<int64_t>("Timestamp", std::move(timestampComputer));
    auto table = builder.build();

    auto names = table.getColumnNames();
    REQUIRE(names.size() == 1);
    REQUIRE(table.hasColumn("Timestamp"));

    auto const & timestamps_col = table.getColumnValues<int64_t>("Timestamp");
    REQUIRE(timestamps_col.size() == 2);
    REQUIRE(timestamps_col[0] == 0);
    REQUIRE(timestamps_col[1] == 1);
}

TEST_CASE("DM - TV - LineTimestampComputer with per-line row expansion", "[LineTimestampComputer][Expansion]") {
    DataManager dm;

    // Timeframe with 5 timestamps
    std::vector<int> timeValues = {0, 1, 2, 3, 4};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    // LineData with varying number of lines per timestamp
    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);

    // t=0: no lines (should be dropped)
    // t=1: one horizontal line from x=0..10
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {0.0f, 0.0f};
        lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);
    }
    // t=2: two lines; l0 horizontal (x 0..10), l1 vertical (y 0..10)
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {0.0f, 0.0f};
        lineData->addAtTime(TimeFrameIndex(2), xs, ys, false);
        std::vector<float> xs2 = {5.0f, 5.0f};
        std::vector<float> ys2 = {0.0f, 10.0f};
        lineData->addAtTime(TimeFrameIndex(2), xs2, ys2, false);
    }
    // t=3: no lines (should be dropped)
    // t=4: one vertical line (y 0..10 at x=2)
    {
        std::vector<float> xs = {2.0f, 2.0f};
        std::vector<float> ys = {0.0f, 10.0f};
        lineData->addAtTime(TimeFrameIndex(4), xs, ys, false);
    }

    lineData->setIdentityContext("ExpLines", dm.getEntityRegistry());
    lineData->rebuildAllEntityIds();

    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"ExpLines"});
    // Register into DataManager so TableView expansion can resolve the line source by name
    dm.setData<LineData>("ExpLines", lineData, TimeKey("time"));

    // Timestamps include empty ones; expansion should drop t=0 and t=3
    std::vector<TimeFrameIndex> timestamps = {
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    // Build table
    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));

    auto timestampComputer = std::make_unique<LineTimestampComputer>(
            std::static_pointer_cast<ILineSource>(lineAdapter),
            std::string{"ExpLines"},
            tf);
    builder.addColumn<int64_t>("Timestamp", std::move(timestampComputer));

    auto table = builder.build();

    // With expansion: expected rows = t1:1 + t2:2 + t4:1 = 4 rows
    REQUIRE(table.getRowCount() == 4);

    // Column names same structure
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 1);
    REQUIRE(table.hasColumn("Timestamp"));

    // Validate per-entity timestamp ordering as inserted:
    // Row 0 -> t=1, the single horizontal line: timestamp = 1
    // Row 1 -> t=2, entity 0 (horizontal): timestamp = 2
    // Row 2 -> t=2, entity 1 (vertical):   timestamp = 2
    // Row 3 -> t=4, the single vertical line at x=2: timestamp = 4
    auto const & timestamps_col = table.getColumnValues<int64_t>("Timestamp");
    REQUIRE(timestamps_col.size() == 4);

    REQUIRE(timestamps_col[0] == 1);
    REQUIRE(timestamps_col[1] == 2);
    REQUIRE(timestamps_col[2] == 2);
    REQUIRE(timestamps_col[3] == 4);
}

TEST_CASE("DM - TV - LineTimestampComputer expansion with coexisting analog column", "[LineTimestampComputer][Expansion][AnalogBroadcast]") {
    DataManager dm;

    std::vector<int> timeValues = {0, 1, 2, 3};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    dm.setTime(TimeKey("test_time"), tf);

    // LineData: only at t=1
    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {1.0f, 1.0f};
        lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);
    }

    lineData->setIdentityContext("MixedLines", dm.getEntityRegistry());
    lineData->rebuildAllEntityIds();

    dm.setData<LineData>("MixedLines", lineData, TimeKey("test_time"));

    // Analog data present at all timestamps: values 0,10,20,30
    std::vector<float> analogVals = {0.f, 10.f, 20.f, 30.f};
    std::vector<TimeFrameIndex> analogTimes = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
    auto analogData = std::make_shared<AnalogTimeSeries>(analogVals, analogTimes);
    dm.setData<AnalogTimeSeries>("AnalogA", analogData, TimeKey("test_time"));

    // Build selector across all timestamps
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));

    // Line timestamp column (expanding)
    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"MixedLines"});
    auto timestampComputer = std::make_unique<LineTimestampComputer>(
            std::static_pointer_cast<ILineSource>(lineAdapter),
            std::string{"MixedLines"},
            tf);
    builder.addColumn<int64_t>("LineTimestamp", std::move(timestampComputer));

    // Analog timestamp value column
    class SimpleTimestampValueComputer : public IColumnComputer<double> {
    public:
        explicit SimpleTimestampValueComputer(std::shared_ptr<IAnalogSource> src)
            : src_(std::move(src)) {}
        [[nodiscard]] std::pair<std::vector<double>, ColumnEntityIds> compute(ExecutionPlan const & plan) const override {
            std::vector<TimeFrameIndex> idx;
            if (plan.hasIndices()) {
                idx = plan.getIndices();
            } else {
                // Build from rows (expanded)
                for (auto const & r: plan.getRows()) idx.push_back(r.timeIndex);
            }
            std::vector<double> out(idx.size(), 0.0);
            // naive: use AnalogDataAdapter semantics: value == index*10
            for (size_t i = 0; i < idx.size(); ++i) out[i] = static_cast<double>(idx[i].getValue() * 10);
            return {out, std::monostate{}};
        }
        [[nodiscard]] auto getSourceDependency() const -> std::string override { return src_ ? src_->getName() : std::string{"AnalogA"}; }

    private:
        std::shared_ptr<IAnalogSource> src_;
    };

    auto analogSrc = dme_ptr->getAnalogSource("AnalogA");
    REQUIRE(analogSrc != nullptr);
    auto analogComp = std::make_unique<SimpleTimestampValueComputer>(analogSrc);
    builder.addColumn<double>("Analog", std::move(analogComp));

    auto table = builder.build();

    // Expect expanded rows keep all timestamps due to coexisting analog column: t=0,1,2,3 -> 4 rows
    // Line columns will have zero for t=0,2,3 where no line exists; analog column has 0,10,20,30
    REQUIRE(table.getRowCount() == 4);
    auto const & lineTimestamps = table.getColumnValues<int64_t>("LineTimestamp");
    auto const & analog = table.getColumnValues<double>("Analog");
    REQUIRE(lineTimestamps.size() == 4);
    REQUIRE(analog.size() == 4);

    // At t=1 (row 1), line exists; others should be zeros for line columns
    REQUIRE(lineTimestamps[0] == 0);// t=0, no line
    REQUIRE(lineTimestamps[1] == 1);// t=1, line exists
    REQUIRE(lineTimestamps[2] == 0);// t=2, no line
    REQUIRE(lineTimestamps[3] == 0);// t=3, no line

    REQUIRE(analog[0] == Catch::Approx(0.0));
    REQUIRE(analog[1] == Catch::Approx(10.0));
    REQUIRE(analog[2] == Catch::Approx(20.0));
    REQUIRE(analog[3] == Catch::Approx(30.0));
}

/**
 * @brief Base test fixture for LineTimestampComputer with realistic line data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Line data representing whisker traces or geometric features
 * - Multiple lines per timestamp for testing entity expansion
 * - Cross-timeframe scenarios for testing timeframe conversion
 */
class LineTimestampTestFixture {
protected:
    LineTimestampTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();

        m_table_registry_ptr = m_data_manager->getTableRegistry();

        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());

        // Populate with test data
        populateWithLineTestData();
    }

    ~LineTimestampTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

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

private:
    std::unique_ptr<DataManager> m_data_manager;
    TableRegistry * m_table_registry_ptr;
    std::unique_ptr<TablePipeline> m_table_pipeline;

    /**
     * @brief Populate the DataManager with line test data
     */
    void populateWithLineTestData() {
        createTimeFrames();
        createWhiskerTraces();
        createGeometricShapes();
    }

    /**
     * @brief Create TimeFrame objects for different data streams
     */
    void createTimeFrames() {
        // Create "whisker_time" timeframe: 0 to 100 (101 points) - whisker tracking at high frequency
        std::vector<int> whisker_time_values(101);
        std::iota(whisker_time_values.begin(), whisker_time_values.end(), 0);
        auto whisker_time_frame = std::make_shared<TimeFrame>(whisker_time_values);
        m_data_manager->setTime(TimeKey("whisker_time"), whisker_time_frame, true);

        // Create "shape_time" timeframe: 0, 10, 20, 30, ..., 100 (11 points) - geometric shapes at lower frequency
        std::vector<int> shape_time_values;
        shape_time_values.reserve(11);
        for (int i = 0; i <= 10; ++i) {
            shape_time_values.push_back(i * 10);
        }
        auto shape_time_frame = std::make_shared<TimeFrame>(shape_time_values);
        m_data_manager->setTime(TimeKey("shape_time"), shape_time_frame, true);
    }

    /**
     * @brief Create whisker trace data (complex curved lines)
     */
    void createWhiskerTraces() {
        // Create whisker traces with varying curvature and length
        auto whisker_lines = std::make_shared<LineData>();

        // Create curved whisker traces at different time points
        for (int t = 10; t <= 90; t += 20) {
            // Primary whisker - curved arc
            std::vector<float> xs, ys;
            for (int i = 0; i <= 20; ++i) {
                float s = static_cast<float>(i) / 20.0f;
                float x = s * 100.0f;
                float y = 20.0f * std::sin(s * 3.14159f / 2.0f) * (1.0f + 0.1f * static_cast<float>(t) / 100.0f);
                xs.push_back(x);
                ys.push_back(y);
            }
            whisker_lines->addAtTime(TimeFrameIndex(t), xs, ys, false);

            // Secondary whisker - smaller arc below
            if (t >= 30) {
                std::vector<float> xs2, ys2;
                for (int i = 0; i <= 15; ++i) {
                    float s = static_cast<float>(i) / 15.0f;
                    float x = s * 75.0f;
                    float y = -10.0f - 15.0f * std::sin(s * 3.14159f / 3.0f);
                    xs2.push_back(x);
                    ys2.push_back(y);
                }
                whisker_lines->addAtTime(TimeFrameIndex(t), xs2, ys2, false);
            }
        }

        whisker_lines->setIdentityContext("WhiskerTraces", m_data_manager->getEntityRegistry());
        whisker_lines->rebuildAllEntityIds();

        m_data_manager->setData<LineData>("WhiskerTraces", whisker_lines, TimeKey("whisker_time"));
    }

    /**
     * @brief Create geometric shape data (simple geometric lines)
     */
    void createGeometricShapes() {
        // Create geometric shapes with different patterns
        auto shape_lines = std::make_shared<LineData>();

        // Square at t=0
        {
            std::vector<float> xs = {0.0f, 10.0f, 10.0f, 0.0f, 0.0f};
            std::vector<float> ys = {0.0f, 0.0f, 10.0f, 10.0f, 0.0f};
            shape_lines->addAtTime(TimeFrameIndex(0), xs, ys, false);
        }

        // Triangle at t=20
        {
            std::vector<float> xs = {5.0f, 10.0f, 0.0f, 5.0f};
            std::vector<float> ys = {0.0f, 10.0f, 10.0f, 0.0f};
            shape_lines->addAtTime(TimeFrameIndex(2), xs, ys, false);
        }

        // Circle (octagon approximation) at t=40
        {
            std::vector<float> xs, ys;
            for (int i = 0; i <= 8; ++i) {
                float angle = static_cast<float>(i) * 2.0f * 3.14159f / 8.0f;
                xs.push_back(5.0f + 5.0f * std::cos(angle));
                ys.push_back(5.0f + 5.0f * std::sin(angle));
            }
            shape_lines->addAtTime(TimeFrameIndex(4), xs, ys, false);
        }

        // Multiple shapes at different times - star at t=60, circle at t=80
        {
            // Star shape at t=60
            std::vector<float> xs1, ys1;
            for (int i = 0; i <= 10; ++i) {
                float angle = static_cast<float>(i) * 2.0f * 3.14159f / 10.0f;
                float radius = (i % 2 == 0) ? 8.0f : 4.0f;
                xs1.push_back(15.0f + radius * std::cos(angle));
                ys1.push_back(15.0f + radius * std::sin(angle));
            }
            shape_lines->addAtTime(TimeFrameIndex(6), xs1, ys1, false);

            // Small circle at t=80
            std::vector<float> xs2, ys2;
            for (int i = 0; i <= 6; ++i) {
                float angle = static_cast<float>(i) * 2.0f * 3.14159f / 6.0f;
                xs2.push_back(25.0f + 3.0f * std::cos(angle));
                ys2.push_back(25.0f + 3.0f * std::sin(angle));
            }
            shape_lines->addAtTime(TimeFrameIndex(8), xs2, ys2, false);
        }

        shape_lines->setIdentityContext("GeometricShapes", m_data_manager->getEntityRegistry());
        shape_lines->rebuildAllEntityIds();

        m_data_manager->setData<LineData>("GeometricShapes", shape_lines, TimeKey("shape_time"));
    }
};

TEST_CASE_METHOD(LineTimestampTestFixture, "DM - TV - LineTimestampComputer with DataManager fixture", "[LineTimestampComputer][DataManager][Fixture]") {

    SECTION("Test with whisker trace data from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the line source from the DataManager
        auto whisker_source = dme->getLineSource("WhiskerTraces");

        REQUIRE(whisker_source != nullptr);

        // Create row selector from timestamps where whisker data exists
        auto whisker_time_frame = dm.getTime(TimeKey("whisker_time"));
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(10), TimeFrameIndex(30), TimeFrameIndex(50), TimeFrameIndex(70), TimeFrameIndex(90)};

        auto row_selector = std::make_unique<TimestampSelector>(timestamps, whisker_time_frame);

        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add LineTimestampComputer
        auto timestamp_computer = std::make_unique<LineTimestampComputer>(
                whisker_source, "WhiskerTraces", whisker_time_frame);
        builder.addColumn<int64_t>("Timestamp", std::move(timestamp_computer));

        // Build the table
        TableView table = builder.build();

        // Verify table structure - 1 column for timestamps
        // Expected rows: t=10(1) + t=30(2) + t=50(2) + t=70(2) + t=90(2) = 9 rows due to entity expansion
        REQUIRE(table.getRowCount() == 9);
        REQUIRE(table.getColumnCount() == 1);

        auto column_names = table.getColumnNames();
        REQUIRE(column_names.size() == 1);
        REQUIRE(table.hasColumn("Timestamp"));

        // Get sample data to verify timestamp values
        auto timestamps_col = table.getColumnValues<int64_t>("Timestamp");

        REQUIRE(timestamps_col.size() == 9);

        // Verify that timestamps match the expected values
        // Row 0: t=10 -> timestamp = 10
        // Rows 1-2: t=30 -> timestamp = 30 (2 entities)
        // Rows 3-4: t=50 -> timestamp = 50 (2 entities)
        // Rows 5-6: t=70 -> timestamp = 70 (2 entities)
        // Rows 7-8: t=90 -> timestamp = 90 (2 entities)
        REQUIRE(timestamps_col[0] == 10);
        REQUIRE(timestamps_col[1] == 30);
        REQUIRE(timestamps_col[2] == 30);
        REQUIRE(timestamps_col[3] == 50);
        REQUIRE(timestamps_col[4] == 50);
        REQUIRE(timestamps_col[5] == 70);
        REQUIRE(timestamps_col[6] == 70);
        REQUIRE(timestamps_col[7] == 90);
        REQUIRE(timestamps_col[8] == 90);
    }

    SECTION("Test with geometric shape data and multiple entities") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the shape source
        auto shape_source = dme->getLineSource("GeometricShapes");
        REQUIRE(shape_source != nullptr);

        // Create row selector for shape timestamps
        auto shape_time_frame = dm.getTime(TimeKey("shape_time"));
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(0), TimeFrameIndex(2), TimeFrameIndex(4), TimeFrameIndex(6)};

        auto row_selector = std::make_unique<TimestampSelector>(timestamps, shape_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add LineTimestampComputer
        auto timestamp_computer = std::make_unique<LineTimestampComputer>(
                shape_source, "GeometricShapes", shape_time_frame);
        builder.addColumn<int64_t>("Timestamp", std::move(timestamp_computer));

        TableView table = builder.build();

        // Should have 4 rows: square(1) + triangle(1) + circle(1) + star(1) = 4 rows
        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 1);

        auto timestamps_col = table.getColumnValues<int64_t>("Timestamp");

        REQUIRE(timestamps_col.size() == 4);

        // Verify timestamps match the expected values
        // t=0 -> timestamp = 0
        // t=2 -> timestamp = 2
        // t=4 -> timestamp = 4
        // t=6 -> timestamp = 6
        REQUIRE(timestamps_col[0] == 0);
        REQUIRE(timestamps_col[1] == 2);
        REQUIRE(timestamps_col[2] == 4);
        REQUIRE(timestamps_col[3] == 6);
    }
}

TEST_CASE_METHOD(LineTimestampTestFixture, "DM - TV - LineTimestampComputer via ComputerRegistry", "[LineTimestampComputer][Registry]") {

    SECTION("Verify LineTimestampComputer is registered in ComputerRegistry") {
        auto & registry = getTableRegistry().getComputerRegistry();

        // Check that LineTimestampComputer is registered
        auto line_timestamp_info = registry.findComputerInfo("Line Timestamp");

        REQUIRE(line_timestamp_info != nullptr);

        // Verify computer info details
        REQUIRE(line_timestamp_info->name == "Line Timestamp");
        REQUIRE(line_timestamp_info->outputType == typeid(int64_t));
        REQUIRE(line_timestamp_info->outputTypeName == "int64_t");
        REQUIRE(line_timestamp_info->requiredRowSelector == RowSelectorType::Timestamp);
        REQUIRE(line_timestamp_info->requiredSourceType == typeid(std::shared_ptr<ILineSource>));
        REQUIRE(line_timestamp_info->isMultiOutput == false);
    }

    SECTION("Create LineTimestampComputer via ComputerRegistry") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        // Get whisker source for testing
        auto whisker_source = dme->getLineSource("WhiskerTraces");
        REQUIRE(whisker_source != nullptr);

        // Create computer via registry
        auto computer = registry.createTypedComputer<int64_t>(
                "Line Timestamp", whisker_source);

        REQUIRE(computer != nullptr);

        // Test that the created computer works correctly
        auto whisker_time_frame = dm.getTime(TimeKey("whisker_time"));

        // Create a simple test
        std::vector<TimeFrameIndex> test_timestamps = {TimeFrameIndex(30)};
        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, whisker_time_frame);

        // Test computer
        {
            TableViewBuilder builder(dme);
            builder.setRowSelector(std::move(row_selector));
            builder.addColumn("RegistryTimestamp", std::move(computer));

            auto table = builder.build();
            REQUIRE(table.getRowCount() == 2);// 2 entities at t=30
            REQUIRE(table.getColumnCount() == 1);

            auto column_names = table.getColumnNames();
            REQUIRE(table.hasColumn("RegistryTimestamp"));

            auto timestamps_col = table.getColumnValues<int64_t>("RegistryTimestamp");
            REQUIRE(timestamps_col.size() == 2);
            REQUIRE(timestamps_col[0] == 30);
            REQUIRE(timestamps_col[1] == 30);
        }
    }

    SECTION("Compare registry-created vs direct-created computers") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        auto whisker_source = dme->getLineSource("WhiskerTraces");
        REQUIRE(whisker_source != nullptr);

        // Create computer via registry
        auto registry_computer = registry.createTypedComputer<int64_t>(
                "Line Timestamp", whisker_source);

        // Create computer directly
        auto whisker_time_frame = dm.getTime(TimeKey("whisker_time"));
        auto direct_computer = std::make_unique<LineTimestampComputer>(
                whisker_source, "WhiskerTraces", whisker_time_frame);

        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);

        // Test both computers with the same data
        std::vector<TimeFrameIndex> test_timestamps = {TimeFrameIndex(50)};

        // Registry computer test
        {
            auto registry_selector = std::make_unique<TimestampSelector>(test_timestamps, whisker_time_frame);
            TableViewBuilder builder(dme);
            builder.setRowSelector(std::move(registry_selector));
            builder.addColumn("Registry", std::move(registry_computer));
            auto registry_table = builder.build();

            auto registry_timestamps = registry_table.getColumnValues<int64_t>("Registry");
            REQUIRE(registry_timestamps.size() == 2);
            REQUIRE(registry_timestamps[0] == 50);
            REQUIRE(registry_timestamps[1] == 50);
        }

        // Direct computer test
        {
            auto direct_selector = std::make_unique<TimestampSelector>(test_timestamps, whisker_time_frame);
            TableViewBuilder builder(dme);
            builder.setRowSelector(std::move(direct_selector));
            builder.addColumn<int64_t>("Direct", std::move(direct_computer));
            auto direct_table = builder.build();

            auto direct_timestamps = direct_table.getColumnValues<int64_t>("Direct");
            REQUIRE(direct_timestamps.size() == 2);
            REQUIRE(direct_timestamps[0] == 50);
            REQUIRE(direct_timestamps[1] == 50);
        }

        std::cout << "Comparison test - Both computers produce identical timestamp values" << std::endl;
    }
}

TEST_CASE_METHOD(LineTimestampTestFixture, "DM - TV - LineTimestampComputer via JSON TablePipeline", "[LineTimestampComputer][JSON][Pipeline]") {

    SECTION("Test basic line timestamp extraction via JSON pipeline") {
        // JSON configuration for testing LineTimestampComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Line Timestamp Test",
                "description": "Test JSON execution of LineTimestampComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "line_timestamp_test",
                    "name": "Line Timestamp Test Table",
                    "description": "Test table using LineTimestampComputer",
                    "row_selector": {
                        "type": "timestamp",
                        "timestamps": [10, 30, 50, 70, 90],
                        "timeframe": "whisker_time"
                    },
                    "columns": [
                        {
                            "name": "WhiskerTimestamp",
                            "description": "Extract timestamps from whisker traces",
                            "data_source": "WhiskerTraces",
                            "computer": "Line Timestamp"
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
        REQUIRE(config.table_id == "line_timestamp_test");
        REQUIRE(config.name == "Line Timestamp Test Table");
        REQUIRE(config.columns.size() == 1);

        // Verify column configuration
        auto const & column = config.columns[0];
        REQUIRE(column["name"] == "WhiskerTimestamp");
        REQUIRE(column["computer"] == "Line Timestamp");
        REQUIRE(column["data_source"] == "WhiskerTraces");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "timestamp");
        auto timestamps = config.row_selector["timestamps"];
        REQUIRE(timestamps.size() == 5);
        REQUIRE(timestamps[0] == 10);
        REQUIRE(timestamps[4] == 90);

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
            REQUIRE(registry.hasTable("line_timestamp_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("line_timestamp_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const & name: column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 1);
            REQUIRE(built_table->hasColumn("WhiskerTimestamp"));

            // Verify table has 9 rows due to entity expansion: t=10(1) + t=30(2) + t=50(2) + t=70(2) + t=90(2) = 9
            REQUIRE(built_table->getRowCount() == 9);

            // Get and verify the computed values
            auto timestamps_col = built_table->getColumnValues<int64_t>("WhiskerTimestamp");

            REQUIRE(timestamps_col.size() == 9);

            // Verify timestamp values match expected pattern
            REQUIRE(timestamps_col[0] == 10);// t=10
            REQUIRE(timestamps_col[1] == 30);// t=30, entity 0
            REQUIRE(timestamps_col[2] == 30);// t=30, entity 1
            REQUIRE(timestamps_col[3] == 50);// t=50, entity 0
            REQUIRE(timestamps_col[4] == 50);// t=50, entity 1
            REQUIRE(timestamps_col[5] == 70);// t=70, entity 0
            REQUIRE(timestamps_col[6] == 70);// t=70, entity 1
            REQUIRE(timestamps_col[7] == 90);// t=90, entity 0
            REQUIRE(timestamps_col[8] == 90);// t=90, entity 1

            std::cout << "✓ All timestamp values match expected pattern" << std::endl;

        } else {
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}
