#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TableViewerWidget.hpp"
#include "PaginatedTableModel.hpp"

// DataManager and TableView related includes
#include "DataManager.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/utils/TableView/core/TableViewBuilder.h"
#include "DataManager/utils/TableView/interfaces/IRowSelector.h"
#include "DataManager/utils/TableView/interfaces/IIntervalSource.h"
#include "DataManager/utils/TableView/interfaces/IEventSource.h"
#include "DataManager/utils/TableView/TableRegistry.hpp"
#include "DataManager/utils/TableView/TableInfo.hpp"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/TimeFrame/TimeFrame.hpp"
#include "DataManager/TimeFrame/interval_data.hpp"
#include "DataManager/utils/TableView/core/ExecutionPlan.h"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/computers/AnalogSliceGathererComputer.h"

// Analog data
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

// Qt includes for widget testing
#include <QApplication>
#include <QTableView>
#include <QAbstractItemModel>

#include <memory>
#include <vector>
#include <algorithm>
#include <numeric>

/**
 * @brief Test fixture for TableViewerWidget that creates test data similar to EventInIntervalComputer tests
 * 
 * This fixture provides:
 * - DataManager with TimeFrames and test data
 * - TableRegistry access
 * - Methods to create TableView objects for testing
 * - Mock event and interval data for realistic testing
 */
class TableViewerWidgetTestFixture {
protected:
    TableViewerWidgetTestFixture() {
        // Initialize Qt application if not already done
        if (!QApplication::instance()) {
            int argc = 0;
            char** argv = nullptr;
            app = std::make_unique<QApplication>(argc, argv);
        }
        
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();
        
        // Populate with test data
        populateWithTestData();
    }

    ~TableViewerWidgetTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

    /**
     * @brief Get the TableRegistry from DataManager
     */
    TableRegistry & getTableRegistry() { return *m_data_manager->getTableRegistry(); }

    /**
     * @brief Get DataManagerExtension for building tables
     */
    std::shared_ptr<DataManagerExtension> getDataManagerExtension() { 
        if (!m_data_manager_extension) {
            m_data_manager_extension = std::make_shared<DataManagerExtension>(*m_data_manager);
        }
        return m_data_manager_extension; 
    }

    /**
     * @brief Create a sample TableView for testing
     * @return Shared pointer to a TableView with test data
     */
    std::shared_ptr<TableView> createSampleTableView() {
        auto dme = getDataManagerExtension();
        
        // Get test data sources
        auto neuron1_source = dme->getEventSource("Neuron1Spikes");
        auto neuron2_source = dme->getEventSource("Neuron2Spikes");
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        
        if (!neuron1_source || !neuron2_source || !behavior_source) {
            return nullptr;
        }
        
        // Create row selector from behavior intervals
        auto behavior_time_frame = m_data_manager->getTime(TimeKey("behavior_time"));
        auto behavior_intervals = behavior_source->getIntervalsInRange(
            TimeFrameIndex(0), TimeFrameIndex(100), behavior_time_frame.get());
        
        std::vector<TimeFrameInterval> row_intervals;
        for (const auto& interval : behavior_intervals) {
            row_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }
        
        auto row_selector = std::make_unique<IntervalSelector>(row_intervals, behavior_time_frame);
        
        // Build TableView with multiple columns
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Add different column types for testing
        builder.addColumn<bool>("Neuron1_Present", 
            std::make_unique<EventInIntervalComputer<bool>>(neuron1_source, 
                EventOperation::Presence, "Neuron1Spikes"));
        
        builder.addColumn<int>("Neuron1_Count", 
            std::make_unique<EventInIntervalComputer<int>>(neuron1_source, 
                EventOperation::Count, "Neuron1Spikes"));
        
        builder.addColumn<bool>("Neuron2_Present", 
            std::make_unique<EventInIntervalComputer<bool>>(neuron2_source, 
                EventOperation::Presence, "Neuron2Spikes"));
        
        builder.addColumn<int>("Neuron2_Count", 
            std::make_unique<EventInIntervalComputer<int>>(neuron2_source, 
                EventOperation::Count, "Neuron2Spikes"));
        
        builder.addColumn<std::vector<float>>("Neuron1_Times", 
            std::make_unique<EventInIntervalComputer<std::vector<float>>>(neuron1_source, 
                EventOperation::Gather, "Neuron1Spikes"));
        
        // Build and return the table
        TableView table = builder.build();
        return std::make_shared<TableView>(std::move(table));
    }

    /**
     * @brief Create column infos for table configuration testing
     */
    std::vector<ColumnInfo> createSampleColumnInfos() {
        std::vector<ColumnInfo> column_infos;
        
        // Create column info for each test column
        ColumnInfo neuron1_present("Neuron1_Present", "Presence of Neuron1 spikes", 
                                  "Neuron1Spikes", "Event Presence");
        neuron1_present.outputType = typeid(bool);
        neuron1_present.outputTypeName = "bool";
        column_infos.push_back(std::move(neuron1_present));
        
        ColumnInfo neuron1_count("Neuron1_Count", "Count of Neuron1 spikes", 
                                "Neuron1Spikes", "Event Count");
        neuron1_count.outputType = typeid(int);
        neuron1_count.outputTypeName = "int";
        column_infos.push_back(std::move(neuron1_count));
        
        ColumnInfo neuron2_present("Neuron2_Present", "Presence of Neuron2 spikes", 
                                  "Neuron2Spikes", "Event Presence");
        neuron2_present.outputType = typeid(bool);
        neuron2_present.outputTypeName = "bool";
        column_infos.push_back(std::move(neuron2_present));
        
        ColumnInfo neuron2_count("Neuron2_Count", "Count of Neuron2 spikes", 
                                "Neuron2Spikes", "Event Count");
        neuron2_count.outputType = typeid(int);
        neuron2_count.outputTypeName = "int";
        column_infos.push_back(std::move(neuron2_count));
        
        ColumnInfo neuron1_times("Neuron1_Times", "Spike times for Neuron1", 
                                "Neuron1Spikes", "Event Gather");
        neuron1_times.outputType = typeid(std::vector<float>);
        neuron1_times.outputTypeName = "std::vector<float>";
        neuron1_times.isVectorType = true;
        neuron1_times.elementType = typeid(float);
        neuron1_times.elementTypeName = "float";
        column_infos.push_back(std::move(neuron1_times));
        
        return column_infos;
    }

    /**
     * @brief Create a row selector for table configuration testing
     */
    std::unique_ptr<IRowSelector> createSampleRowSelector() {
        auto dme = getDataManagerExtension();
        auto behavior_source = dme->getIntervalSource("BehaviorPeriods");
        
        if (!behavior_source) {
            return nullptr;
        }
        
        auto behavior_time_frame = m_data_manager->getTime(TimeKey("behavior_time"));
        auto behavior_intervals = behavior_source->getIntervalsInRange(
            TimeFrameIndex(0), TimeFrameIndex(100), behavior_time_frame.get());
        
        std::vector<TimeFrameInterval> row_intervals;
        for (const auto& interval : behavior_intervals) {
            row_intervals.emplace_back(TimeFrameIndex(interval.start), TimeFrameIndex(interval.end));
        }
        
        return std::make_unique<IntervalSelector>(row_intervals, behavior_time_frame);
    }

private:
    std::unique_ptr<DataManager> m_data_manager;
    std::shared_ptr<DataManagerExtension> m_data_manager_extension;
    std::unique_ptr<QApplication> app;

    /**
     * @brief Populate the DataManager with test data (similar to EventInIntervalComputer tests)
     */
    void populateWithTestData() {
        createTimeFrames();
        createBehaviorIntervals();
        createSpikeEvents();
    }

    /**
     * @brief Create TimeFrame objects for different data streams
     */
    void createTimeFrames() {
        // Create "behavior_time" timeframe: 0 to 100 (101 points)
        std::vector<int> behavior_time_values(101);
        std::iota(behavior_time_values.begin(), behavior_time_values.end(), 0);
        auto behavior_time_frame = std::make_shared<TimeFrame>(behavior_time_values);
        m_data_manager->setTime(TimeKey("behavior_time"), behavior_time_frame, true);

        // Create "spike_time" timeframe: 0, 2, 4, 6, ..., 100 (51 points)
        std::vector<int> spike_time_values;
        spike_time_values.reserve(51);
        for (int i = 0; i <= 50; ++i) {
            spike_time_values.push_back(i * 2);
        }
        auto spike_time_frame = std::make_shared<TimeFrame>(spike_time_values);
        m_data_manager->setTime(TimeKey("spike_time"), spike_time_frame, true);
    }

    /**
     * @brief Create behavior intervals (row intervals for testing)
     */
    void createBehaviorIntervals() {
        auto behavior_intervals = std::make_shared<DigitalIntervalSeries>();
        
        // Create 4 behavior periods for testing
        behavior_intervals->addEvent(TimeFrameIndex(10), TimeFrameIndex(25));  // Exploration 1
        behavior_intervals->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));  // Rest
        behavior_intervals->addEvent(TimeFrameIndex(50), TimeFrameIndex(70));  // Exploration 2
        behavior_intervals->addEvent(TimeFrameIndex(80), TimeFrameIndex(95));  // Social

        m_data_manager->setData<DigitalIntervalSeries>("BehaviorPeriods", behavior_intervals, TimeKey("behavior_time"));
    }

    /**
     * @brief Create spike event data (event data for testing)
     */
    void createSpikeEvents() {
        // Create spike train for Neuron1
        std::vector<float> neuron1_spikes = {
            1.0f, 6.0f, 7.0f, 11.0f, 16.0f, 26.0f, 27.0f, 34.0f, 41.0f, 45.0f
        };
        auto neuron1_series = std::make_shared<DigitalEventSeries>(neuron1_spikes);
        m_data_manager->setData<DigitalEventSeries>("Neuron1Spikes", neuron1_series, TimeKey("spike_time"));
        
        // Create spike train for Neuron2
        std::vector<float> neuron2_spikes = {
            0.0f, 1.0f, 2.0f, 5.0f, 6.0f, 8.0f, 9.0f, 15.0f, 16.0f, 18.0f,
            25.0f, 26.0f, 28.0f, 29.0f, 33.0f, 34.0f, 40.0f, 41.0f, 42.0f, 45.0f, 46.0f
        };
        auto neuron2_series = std::make_shared<DigitalEventSeries>(neuron2_spikes);
        m_data_manager->setData<DigitalEventSeries>("Neuron2Spikes", neuron2_series, TimeKey("spike_time"));
    }
};

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Basic Functionality", "[TableViewerWidget]") {
    
    SECTION("Create widget and verify initial state") {
        TableViewerWidget widget;
        
        // Verify initial state
        REQUIRE(!widget.hasTable());
        REQUIRE(widget.getTableName().isEmpty());
        
        // Widget should be properly constructed
        REQUIRE(widget.isWidgetType());
    }
    
    SECTION("Set table view and verify row/column counts") {
        TableViewerWidget widget;
        
        // Create a sample table
        auto table_view = createSampleTableView();
        REQUIRE(table_view != nullptr);
        
        // Get original table dimensions
        size_t original_row_count = table_view->getRowCount();
        size_t original_column_count = table_view->getColumnCount();
        
        // Set the table in the widget
        widget.setTableView(table_view, "Test Table");
        
        // Verify widget state
        REQUIRE(widget.hasTable());
        REQUIRE(widget.getTableName() == "Test Table");
        
        // Access the internal model to verify dimensions
        // Note: We need to access the QTableView to get the model
        auto* table_view_widget = widget.findChild<QTableView*>();
        REQUIRE(table_view_widget != nullptr);
        
        auto* model = table_view_widget->model();
        REQUIRE(model != nullptr);
        
        // Verify the model has the correct dimensions
        REQUIRE(static_cast<size_t>(model->columnCount()) == original_column_count);
        REQUIRE(static_cast<size_t>(model->rowCount()) == original_row_count);
        
        // Verify expected dimensions based on our test data
        REQUIRE(original_row_count == 4);     // 4 behavior periods
        REQUIRE(original_column_count == 5);  // 5 columns we added
    }
    
    SECTION("Set table configuration and verify row/column counts") {
        TableViewerWidget widget;
        
        // Create configuration components
        auto row_selector = createSampleRowSelector();
        auto column_infos = createSampleColumnInfos();
        auto data_manager = std::shared_ptr<DataManager>(getDataManagerPtr(), [](DataManager*){});
        
        REQUIRE(row_selector != nullptr);
        REQUIRE(!column_infos.empty());
        
        // Get expected dimensions before setting
        size_t expected_columns = column_infos.size();
        
        // Calculate expected rows by counting intervals in the row selector
        // For our test data, we should have 4 behavior periods
        size_t expected_rows = 4;
        
        // Set the table configuration
        widget.setTableConfiguration(std::move(row_selector), std::move(column_infos), 
                                   data_manager, "Configuration Test");
        
        // Verify widget state
        REQUIRE(widget.hasTable());
        REQUIRE(widget.getTableName() == "Configuration Test");
        
        // Access the internal model to verify dimensions
        auto* table_view_widget = widget.findChild<QTableView*>();
        REQUIRE(table_view_widget != nullptr);
        
        auto* model = table_view_widget->model();
        REQUIRE(model != nullptr);
        
        // Verify the model has the correct dimensions
        REQUIRE(static_cast<size_t>(model->columnCount()) == expected_columns);
        REQUIRE(static_cast<size_t>(model->rowCount()) == expected_rows);
        
        // Verify expected dimensions
        REQUIRE(expected_columns == 5);  // 5 columns we defined
        REQUIRE(expected_rows == 4);     // 4 behavior periods
    }
}

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Display vector column (AnalogSliceGathererComputer)", "[TableViewerWidget][VectorDisplay]") {
    auto & dm = getDataManager();
    auto dme = getDataManagerExtension();

    // Timeframe 0..9
    std::vector<int> time_vals(10);
    std::iota(time_vals.begin(), time_vals.end(), 0);
    auto tf = std::make_shared<TimeFrame>(time_vals);
    dm.setTime(TimeKey("vec_time"), tf, true);

    // Analog 0..9 at each index
    std::vector<float> vals(10);
    std::iota(vals.begin(), vals.end(), 0.0f);
    std::vector<TimeFrameIndex> tix; tix.reserve(10);
    for (int i = 0; i < 10; ++i) tix.emplace_back(i);
    auto ats = std::make_shared<AnalogTimeSeries>(vals, tix);
    dm.setData<AnalogTimeSeries>("VecAnalog", ats, TimeKey("vec_time"));

    // Two intervals: [2,4] and [6,8]
    std::vector<TimeFrameInterval> intervals;
    intervals.emplace_back(TimeFrameIndex(2), TimeFrameIndex(4));
    intervals.emplace_back(TimeFrameIndex(6), TimeFrameIndex(8));
    auto row_selector = std::make_unique<IntervalSelector>(intervals, tf);

    // Build table with AnalogSliceGathererComputer<double>
    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(row_selector));
    auto analog_src = dme->getAnalogSource("VecAnalog");
    REQUIRE(analog_src != nullptr);
    builder.addColumn<std::vector<double>>("Slices", std::make_unique<AnalogSliceGathererComputer<std::vector<double>>>(analog_src, "VecAnalog"));
    TableView table = builder.build();
    auto table_view = std::make_shared<TableView>(std::move(table));

    // Show in widget
    TableViewerWidget widget;
    widget.setTableView(table_view, "Vector Column Test");
    REQUIRE(widget.hasTable());

    auto * tv = widget.findChild<QTableView*>();
    REQUIRE(tv != nullptr);
    auto * model = tv->model();
    REQUIRE(model != nullptr);

    REQUIRE(model->rowCount() == 2);
    REQUIRE(model->columnCount() == 1);

    // Verify formatted display strings (comma-separated, 3 decimals)
    auto row0 = model->data(model->index(0, 0), Qt::DisplayRole).toString();
    auto row1 = model->data(model->index(1, 0), Qt::DisplayRole).toString();
    REQUIRE(row0 == QString("2.000,3.000,4.000"));
    REQUIRE(row1 == QString("6.000,7.000,8.000"));
}

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Pagination with analog timestamps", "[TableViewerWidget][Pagination][Analog]") {
    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    // Parameters
    const int num_rows = 2500;      // triggers pagination
    const int page_size = 64;       // small to force multiple pages

    // Create a monotonic analog signal: value(t) = t for t in [0, num_rows)
    auto time_frame = std::make_shared<TimeFrame>([&]{
        std::vector<int> t(num_rows);
        std::iota(t.begin(), t.end(), 0);
        return t;
    }());
    getDataManager().setTime(TimeKey("time"), time_frame, true);

    std::vector<float> values(static_cast<size_t>(num_rows));
    std::iota(values.begin(), values.end(), 0.0f);
    std::vector<TimeFrameIndex> indices;
    for (int i = 0; i < num_rows; ++i) indices.push_back(TimeFrameIndex(i));
    auto ats = std::make_shared<AnalogTimeSeries>(values, indices);
    getDataManager().setData<AnalogTimeSeries>("Iota", ats, TimeKey("time"));

    // Build Timestamp row selector 0..num_rows-1 in the same timeframe
    std::vector<TimeFrameIndex> row_timestamps(indices.begin(), indices.end());
    auto row_selector = std::make_unique<TimestampSelector>(std::move(row_timestamps), time_frame);

    // One column sampling analog at timestamps
    std::vector<ColumnInfo> column_infos;
    ColumnInfo info("IotaValue", "Analog sample at timestamp", "analog:Iota", "Timestamp Value");
    info.outputType = typeid(double);
    info.outputTypeName = "double";
    column_infos.push_back(std::move(info));

    // Wire into widget using pagination-backed API
    TableViewerWidget widget;
    widget.setPageSize(static_cast<size_t>(page_size));
    auto data_manager = std::shared_ptr<DataManager>(getDataManagerPtr(), [](DataManager*){});
    widget.setTableConfiguration(std::move(row_selector), std::move(column_infos), data_manager, "Analog Pagination");

    REQUIRE(widget.hasTable());

    auto * table_view_widget = widget.findChild<QTableView*>();
    REQUIRE(table_view_widget != nullptr);
    auto * model = table_view_widget->model();
    REQUIRE(model != nullptr);

    REQUIRE(model->rowCount() == num_rows);
    REQUIRE(model->columnCount() == 1);

    auto expectValueAt = [&](int row) {
        auto idx = model->index(row, 0);
        REQUIRE(idx.isValid());
        auto v = model->data(idx, Qt::DisplayRole).toString();
        // "Timestamp Value" returns double formatted to 3 decimals by model
        QString expected = QString::number(static_cast<double>(row), 'f', 3);
        REQUIRE(v == expected);
    };

    // Probe around page boundaries
    expectValueAt(0);
    expectValueAt(page_size - 1);
    expectValueAt(page_size);
    expectValueAt(page_size * 2);
    expectValueAt(250);
    expectValueAt(1024);
    expectValueAt(2048);
}

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Resize increases visible rows (lazy fill)", "[TableViewerWidget][Pagination][Resize]") {
    // Build small analog table similar to previous test
    const int num_rows = 1000;
    const int page_size = 64;

    auto time_frame = std::make_shared<TimeFrame>([&]{
        std::vector<int> t(num_rows);
        std::iota(t.begin(), t.end(), 0);
        return t;
    }());
    getDataManager().setTime(TimeKey("time_resize"), time_frame, true);

    std::vector<float> values(static_cast<size_t>(num_rows));
    std::iota(values.begin(), values.end(), 0.0f);
    std::vector<TimeFrameIndex> indices;
    indices.reserve(static_cast<size_t>(num_rows));
    for (int i = 0; i < num_rows; ++i) indices.push_back(TimeFrameIndex(i));
    auto ats = std::make_shared<AnalogTimeSeries>(values, indices);
    getDataManager().setData<AnalogTimeSeries>("IotaResize", ats, TimeKey("time_resize"));

    std::vector<TimeFrameIndex> row_timestamps(indices.begin(), indices.end());
    auto row_selector = std::make_unique<TimestampSelector>(std::move(row_timestamps), time_frame);

    std::vector<ColumnInfo> column_infos;
    ColumnInfo info("Val", "Analog at t", "analog:IotaResize", "Timestamp Value");
    info.outputType = typeid(double);
    info.outputTypeName = "double";
    column_infos.push_back(std::move(info));

    TableViewerWidget widget;
    widget.setPageSize(static_cast<size_t>(page_size));
    auto data_manager = std::shared_ptr<DataManager>(getDataManagerPtr(), [](DataManager*){});
    widget.setTableConfiguration(std::move(row_selector), std::move(column_infos), data_manager, "Resize Test");

    auto * table_view = widget.findChild<QTableView*>();
    REQUIRE(table_view != nullptr);
    auto * model = table_view->model();
    REQUIRE(model != nullptr);

    // Initial small size to limit visible rows
    widget.resize(400, 200);
    widget.show();
    QApplication::processEvents();

    // Determine top/visible region
    QModelIndex top_idx = table_view->indexAt(table_view->rect().topLeft());
    int top_row = top_idx.isValid() ? top_idx.row() : 0;
    // Sample a bottom-ish row within current viewport (heuristic)
    int sample_row_before = std::min(top_row + 20, num_rows - 1);
    auto before_idx = model->index(sample_row_before, 0);
    QVariant before_val = model->data(before_idx, Qt::DisplayRole);
    REQUIRE(before_val.isValid());

    // Resize larger: more rows exposed
    widget.resize(400, 800);
    QApplication::processEvents();

    // New region likely exposes more rows; check one that should newly appear
    QModelIndex new_top_idx = table_view->indexAt(table_view->rect().topLeft());
    int new_top_row = new_top_idx.isValid() ? new_top_idx.row() : 0;
    int sample_row_after = std::min(new_top_row + 60, num_rows - 1);
    auto after_idx = model->index(sample_row_after, 0);
    auto after_val = model->data(after_idx, Qt::DisplayRole);

    // Accept either expected formatted value or temporarily empty while mini-page materializes
    QString expected = QString::number(static_cast<double>(sample_row_after), 'f', 3);
    REQUIRE((!after_val.isValid() || after_val.toString().isEmpty() || after_val.toString() == expected));

    // After processing events again, value should materialize
    QApplication::processEvents();
    after_val = model->data(after_idx, Qt::DisplayRole);
    REQUIRE(after_val.isValid());
    REQUIRE(after_val.toString() == expected);
}

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Table Registry Integration", "[TableViewerWidget][Registry]") {
    
    SECTION("Verify table registry provides correct table access") {
        auto& registry = getTableRegistry();
        auto& dm = getDataManager();
        
        // Create a table using the registry's computers
        auto dme = getDataManagerExtension();
        
        // Build a table using registry components
        auto row_selector = createSampleRowSelector();
        REQUIRE(row_selector != nullptr);
        
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        
        // Get neuron sources
        auto neuron1_source = dme->getEventSource("Neuron1Spikes");
        auto neuron2_source = dme->getEventSource("Neuron2Spikes");
        REQUIRE(neuron1_source != nullptr);
        REQUIRE(neuron2_source != nullptr);
        
        // Use registry to create computers
        auto& computer_registry = registry.getComputerRegistry();
        
        std::map<std::string, std::string> empty_params;
        
        auto presence_computer1 = computer_registry.createTypedComputer<bool>(
            "Event Presence", neuron1_source, empty_params);
        auto count_computer1 = computer_registry.createTypedComputer<int>(
            "Event Count", neuron1_source, empty_params);
        auto presence_computer2 = computer_registry.createTypedComputer<bool>(
            "Event Presence", neuron2_source, empty_params);
        auto count_computer2 = computer_registry.createTypedComputer<int>(
            "Event Count", neuron2_source, empty_params);
        
        REQUIRE(presence_computer1 != nullptr);
        REQUIRE(count_computer1 != nullptr);
        REQUIRE(presence_computer2 != nullptr);
        REQUIRE(count_computer2 != nullptr);
        
        // Add columns using registry-created computers
        builder.addColumn("Registry_N1_Present", std::move(presence_computer1));
        builder.addColumn("Registry_N1_Count", std::move(count_computer1));
        builder.addColumn("Registry_N2_Present", std::move(presence_computer2));
        builder.addColumn("Registry_N2_Count", std::move(count_computer2));
        
        // Build the table
        TableView registry_table = builder.build();
        auto table_view = std::make_shared<TableView>(std::move(registry_table));
        
        // Test with TableViewerWidget
        TableViewerWidget widget;
        widget.setTableView(table_view, "Registry Table");
        
        // Verify dimensions
        REQUIRE(widget.hasTable());
        REQUIRE(widget.getTableName() == "Registry Table");
        
        auto* table_view_widget = widget.findChild<QTableView*>();
        REQUIRE(table_view_widget != nullptr);
        
        auto* model = table_view_widget->model();
        REQUIRE(model != nullptr);
        
        // Verify correct number of columns and rows
        REQUIRE(model->columnCount() == 4);  // 4 registry-created columns
        REQUIRE(model->rowCount() == 4);     // 4 behavior periods
        
        // Verify these match the original table
        REQUIRE(static_cast<size_t>(model->columnCount()) == table_view->getColumnCount());
        REQUIRE(static_cast<size_t>(model->rowCount()) == table_view->getRowCount());
    }
}

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Data Consistency", "[TableViewerWidget][Data]") {
    
    SECTION("Verify table data matches source table") {
        TableViewerWidget widget;
        
        // Create a sample table
        auto table_view = createSampleTableView();
        REQUIRE(table_view != nullptr);
        
        // Get some reference data from the original table
        auto original_columns = table_view->getColumnNames();
        auto original_neuron1_present = table_view->getColumnValues<bool>("Neuron1_Present");
        auto original_neuron1_count = table_view->getColumnValues<int>("Neuron1_Count");
        
        // Set the table in the widget
        widget.setTableView(table_view, "Data Test");
        
        // Access the model to verify data consistency
        auto* table_view_widget = widget.findChild<QTableView*>();
        REQUIRE(table_view_widget != nullptr);
        
        auto* model = table_view_widget->model();
        REQUIRE(model != nullptr);
        
        // Verify column headers match
        for (int col = 0; col < model->columnCount(); ++col) {
            QString header = model->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
            REQUIRE(!header.isEmpty());
        }
        
        // Verify we can access data without errors
        for (int row = 0; row < model->rowCount(); ++row) {
            for (int col = 0; col < model->columnCount(); ++col) {
                QVariant data = model->data(model->index(row, col), Qt::DisplayRole);
                // Data should be valid (even if empty string for complex types)
                REQUIRE(data.isValid());
            }
        }
        
        // Verify dimensions match expectations
        REQUIRE(model->rowCount() == static_cast<int>(original_neuron1_present.size()));
        REQUIRE(model->rowCount() == static_cast<int>(original_neuron1_count.size()));
        REQUIRE(model->columnCount() == static_cast<int>(original_columns.size()));
    }
}

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Widget Lifecycle", "[TableViewerWidget][Lifecycle]") {
    
    SECTION("Clear table and verify state reset") {
        TableViewerWidget widget;
        
        // Set a table
        auto table_view = createSampleTableView();
        widget.setTableView(table_view, "Test Table");
        
        // Verify table is set
        REQUIRE(widget.hasTable());
        REQUIRE(widget.getTableName() == "Test Table");
        
        // Clear the table
        widget.clearTable();
        
        // Verify state is reset
        REQUIRE(!widget.hasTable());
        REQUIRE(widget.getTableName().isEmpty());
        
        // Model should still exist but be empty or reset
        auto* table_view_widget = widget.findChild<QTableView*>();
        REQUIRE(table_view_widget != nullptr);
        
        auto* model = table_view_widget->model();
        // Model might be null or have 0 rows/columns after clearing
        if (model != nullptr) {
            // If model exists, it should have no data
            REQUIRE((model->rowCount() == 0 || model->columnCount() == 0));
        }
    }
    
    SECTION("Set page size and verify it's applied") {
        TableViewerWidget widget;
        
        // Set page size before setting table
        widget.setPageSize(100);
        
        // Create configuration components
        auto row_selector = createSampleRowSelector();
        auto column_infos = createSampleColumnInfos();
        auto data_manager = std::shared_ptr<DataManager>(getDataManagerPtr(), [](DataManager*){});
        
        // Set table configuration
        widget.setTableConfiguration(std::move(row_selector), std::move(column_infos), 
                                   data_manager, "Page Size Test");
        
        // Verify table is set and working
        REQUIRE(widget.hasTable());
        
        // With our small test dataset (4 rows), page size shouldn't affect visibility
        auto* table_view_widget = widget.findChild<QTableView*>();
        REQUIRE(table_view_widget != nullptr);
        
        auto* model = table_view_widget->model();
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount() == 4);  // All rows should be visible
    }
}

TEST_CASE_METHOD(TableViewerWidgetTestFixture, "TableViewerWidget - Pagination with large table", "[TableViewerWidget][Pagination]") {
    // Build a large pagination-backed configuration: 2500 rows and 1000 columns
    
    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    const int num_rows = 2500;
    const int num_columns = 10;

    auto dme = getDataManagerExtension();
    REQUIRE(dme != nullptr);

    // Create many intervals (rows) using existing behavior_time timeframe (0..100)
    auto behavior_time_frame = getDataManager().getTime(TimeKey("behavior_time"));
    std::vector<TimeFrameInterval> row_intervals;
    row_intervals.reserve(num_rows);
    for (size_t i = 0; i < num_rows; ++i) {
        auto start = TimeFrameIndex(static_cast<int>(i % 100));
        auto end = TimeFrameIndex(static_cast<int>((i % 100) + 1));
        row_intervals.emplace_back(start, end);
    }

    auto row_selector = std::make_unique<IntervalSelector>(row_intervals, behavior_time_frame);

    // Create column infos alternating presence and count from event source "Neuron1Spikes"
    std::vector<ColumnInfo> column_infos;
    column_infos.reserve(num_columns);
    for (int i = 0; i < num_columns; ++i) {
        if (i % 2 == 0) {
            ColumnInfo info("Col_" + std::to_string(i), "Presence", "Neuron1Spikes", "Event Presence");
            info.outputType = typeid(bool);
            info.outputTypeName = "bool";
            column_infos.push_back(std::move(info));
        } else {
            ColumnInfo info("Col_" + std::to_string(i), "Count", "Neuron1Spikes", "Event Count");
            info.outputType = typeid(int);
            info.outputTypeName = "int";
            column_infos.push_back(std::move(info));
        }
    }

    // Use the widget's pagination-backed API
    TableViewerWidget widget;
    widget.setPageSize(64);
    auto data_manager = std::shared_ptr<DataManager>(getDataManagerPtr(), [](DataManager*){});
    widget.setTableConfiguration(std::move(row_selector), std::move(column_infos), data_manager, "Large Pagination Table");

    REQUIRE(widget.hasTable());

    auto * table_view_widget = widget.findChild<QTableView*>();
    REQUIRE(table_view_widget != nullptr);
    auto * base_model = table_view_widget->model();
    REQUIRE(base_model != nullptr);

    // Confirm dimensions
    REQUIRE(base_model->rowCount() == num_rows);
    REQUIRE(base_model->columnCount() == num_columns);

    // Access underlying model to track materialized pages
    auto * model = dynamic_cast<PaginatedTableModel *>(base_model);
    REQUIRE(model != nullptr);

    auto accessRow = [&](int row) {
        for (int col = 0; col < std::min(5, base_model->columnCount()); ++col) {
            auto idx = base_model->index(row, col);
            REQUIRE(idx.isValid());
            auto val = base_model->data(idx, Qt::DisplayRole);
            REQUIRE(val.isValid());
        }
    };

    size_t initial_pages = model->getMaterializedPageCount();

    // Trigger access across multiple page boundaries
    accessRow(0);    // page 0
    accessRow(63);   // still page 0
    accessRow(64);   // page 1
    accessRow(127);  // page 1
    accessRow(128);  // page 2
    accessRow(512);  // deeper page
    accessRow(1024); // deeper page
    accessRow(2048); // deeper page

    size_t pages_after = model->getMaterializedPageCount();
    REQUIRE(pages_after >= initial_pages + 4);

    // Also simulate scrolling the view and then accessing data at the new top
    table_view_widget->scrollTo(base_model->index(1500, 0), QAbstractItemView::PositionAtTop);
    accessRow(1500);
}