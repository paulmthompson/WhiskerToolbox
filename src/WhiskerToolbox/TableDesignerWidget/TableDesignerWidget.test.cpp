#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TableDesignerWidget.hpp"

// DataManager and related includes
#include "DataManager.hpp"
#include "DataManager/utils/TableView/adapters/DataManagerExtension.h"
#include "DataManager/utils/TableView/TableRegistry.hpp"
#include "DataManager/utils/TableView/ComputerRegistry.hpp"
#include "DataManager/utils/TableView/computers/EventInIntervalComputer.h"
#include "DataManager/utils/TableView/computers/AnalogSliceGathererComputer.h"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/TimeFrame/TimeFrame.hpp"

// Qt includes for widget testing
#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QTableView>
#include <QHeaderView>
#include <QTextEdit>
#include <QPushButton>
#include <QTemporaryFile>
#include <QDir>
#include "TableJSONWidget.hpp"
#include <QMessageBox>

#include <memory>
#include <vector>
#include <algorithm>
#include <numeric>

/**
 * @brief Test fixture for TableDesignerWidget that creates test data and computers
 * 
 * This fixture provides:
 * - DataManager with TimeFrames and test data
 * - TableRegistry with registered computers
 * - Methods to verify tree widget functionality
 */
class TableDesignerWidgetTestFixture {
protected:
    TableDesignerWidgetTestFixture() {
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
        
        // Register computers in the registry
        registerTestComputers();
    }

    ~TableDesignerWidgetTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    std::shared_ptr<DataManager> getDataManagerPtr() { 
        return std::shared_ptr<DataManager>(m_data_manager.get(), [](DataManager*){}); 
    }

    /**
     * @brief Get the TableRegistry from DataManager
     */
    TableRegistry & getTableRegistry() { return *m_data_manager->getTableRegistry(); }

private:
    std::unique_ptr<DataManager> m_data_manager;
    std::unique_ptr<QApplication> app;

    /**
     * @brief Populate the DataManager with test data
     */
    void populateWithTestData() {
        createTimeFrames();
        createBehaviorIntervals();
        createSpikeEvents();
        createAnalogData();
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

        // Create "analog_time" timeframe: 0 to 200 (201 points, higher resolution)
        std::vector<int> analog_time_values(201);
        std::iota(analog_time_values.begin(), analog_time_values.end(), 0);
        auto analog_time_frame = std::make_shared<TimeFrame>(analog_time_values);
        m_data_manager->setTime(TimeKey("analog_time"), analog_time_frame, true);
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
     * @brief Create spike event data
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

    /**
     * @brief Create analog data for testing
     */
    void createAnalogData() {
        // Create LFP signal (sine wave)
        std::vector<float> lfp_values;
        std::vector<TimeFrameIndex> lfp_indices;
        lfp_values.reserve(201);
        lfp_indices.reserve(201);
        
        for (int i = 0; i < 201; ++i) {
            float value = std::sin(2.0f * M_PI * i / 50.0f); // 4 cycles over 200 points
            lfp_values.push_back(value);
            lfp_indices.emplace_back(i);
        }
        
        auto lfp_series = std::make_shared<AnalogTimeSeries>(lfp_values, lfp_indices);
        m_data_manager->setData<AnalogTimeSeries>("LFP", lfp_series, TimeKey("analog_time"));

        // Create EMG signal (random noise)
        std::vector<float> emg_values;
        std::vector<TimeFrameIndex> emg_indices;
        emg_values.reserve(201);
        emg_indices.reserve(201);
        
        std::srand(12345); // Deterministic random for testing
        for (int i = 0; i < 201; ++i) {
            float value = static_cast<float>(std::rand()) / RAND_MAX - 0.5f; // [-0.5, 0.5]
            emg_values.push_back(value);
            emg_indices.emplace_back(i);
        }
        
        auto emg_series = std::make_shared<AnalogTimeSeries>(emg_values, emg_indices);
        m_data_manager->setData<AnalogTimeSeries>("EMG", emg_series, TimeKey("analog_time"));
    }

    /**
     * @brief Register test computers in the computer registry
     */
    void registerTestComputers() {
        auto& registry = getTableRegistry();
        auto& computer_registry = registry.getComputerRegistry();
    }
};

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Basic tree functionality", "[TableDesignerWidget][Tree]") {
    
    SECTION("Create widget and verify tree is populated") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        // Get the computers tree
        auto* tree = widget.findChild<QTreeWidget*>("computers_tree");
        REQUIRE(tree != nullptr);
        
        // Verify tree has been populated with data sources
        REQUIRE(tree->topLevelItemCount() > 0);
        
        // Check for expected data source categories
        QStringList found_sources;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto* item = tree->topLevelItem(i);
            found_sources << item->text(0);
        }
        
        // Should have event sources
        bool has_neuron1_events = false;
        bool has_neuron2_events = false;
        bool has_behavior_intervals = false;
        bool has_analog_sources = false;
        
        for (const QString& source : found_sources) {
            if (source.contains("Neuron1Spikes")) has_neuron1_events = true;
            if (source.contains("Neuron2Spikes")) has_neuron2_events = true;
            if (source.contains("BehaviorPeriods")) has_behavior_intervals = true;
            if (source.contains("LFP") || source.contains("EMG")) has_analog_sources = true;
        }
        
        REQUIRE(has_neuron1_events);
        REQUIRE(has_neuron2_events);
        REQUIRE(has_behavior_intervals);
        REQUIRE(has_analog_sources);
    }
    
    SECTION("Verify tree structure - data sources have computer children") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        auto* tree = widget.findChild<QTreeWidget*>("computers_tree");
        REQUIRE(tree != nullptr);
        
        // Find an event source and verify it has event computers
        QTreeWidgetItem* event_source_item = nullptr;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto* item = tree->topLevelItem(i);
            if (item->text(0).contains("Events: Neuron1Spikes")) {
                event_source_item = item;
                break;
            }
        }
        
        REQUIRE(event_source_item != nullptr);
        REQUIRE(event_source_item->childCount() > 0);
        
        // Check that children are computers with checkboxes
        bool has_presence_computer = false;
        bool has_count_computer = false;
        
        for (int j = 0; j < event_source_item->childCount(); ++j) {
            auto* computer_item = event_source_item->child(j);
            
            // Verify computer items have checkboxes in column 1
            REQUIRE((computer_item->flags() & Qt::ItemIsUserCheckable) != 0);
            REQUIRE(computer_item->checkState(1) == Qt::Unchecked); // Initially unchecked
            
            // Check for expected computers
            QString computer_name = computer_item->text(0);
            if (computer_name.contains("Event Presence")) has_presence_computer = true;
            if (computer_name.contains("Event Count")) has_count_computer = true;
            
            // Verify column name is editable and has default value
            REQUIRE((computer_item->flags() & Qt::ItemIsEditable) != 0);
            REQUIRE(!computer_item->text(2).isEmpty()); // Should have default column name
        }
        
        REQUIRE(has_presence_computer);
        REQUIRE(has_count_computer);
    }
    
    SECTION("Verify analog sources have analog computers") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        auto* tree = widget.findChild<QTreeWidget*>("computers_tree");
        REQUIRE(tree != nullptr);
        
        // Find an analog source
        QTreeWidgetItem* analog_source_item = nullptr;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto* item = tree->topLevelItem(i);
            if (item->text(0).contains("analog:LFP")) {
                analog_source_item = item;
                break;
            }
        }
        
        REQUIRE(analog_source_item != nullptr);
        REQUIRE(analog_source_item->childCount() > 0);
        
        // Check for analog computers
        bool has_slice_gatherer = false;
        bool has_mean_computer = false;
        
        for (int j = 0; j < analog_source_item->childCount(); ++j) {
            auto* computer_item = analog_source_item->child(j);
            QString computer_name = computer_item->text(0);
            
            if (computer_name.contains("Analog Slice Gatherer")) has_slice_gatherer = true;
            if (computer_name.contains("Analog Mean")) has_mean_computer = true;
        }
        
        // Should have at least some analog computers
        REQUIRE((has_slice_gatherer || has_mean_computer));
    }
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Computer enabling and column generation", "[TableDesignerWidget][Computers]") {
    
    SECTION("Enable computers and verify column info generation") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        auto* tree = widget.findChild<QTreeWidget*>("computers_tree");
        REQUIRE(tree != nullptr);
        
        // Find and enable some computers
        QTreeWidgetItem* presence_computer = nullptr;
        QTreeWidgetItem* count_computer = nullptr;
        
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto* data_source_item = tree->topLevelItem(i);
            if (!data_source_item->text(0).contains("Events: Neuron1Spikes")) continue;
            
            for (int j = 0; j < data_source_item->childCount(); ++j) {
                auto* computer_item = data_source_item->child(j);
                QString computer_name = computer_item->text(0);
                
                if (computer_name.contains("Event Presence")) {
                    presence_computer = computer_item;
                }
                if (computer_name.contains("Event Count")) {
                    count_computer = computer_item;
                }
            }
        }
        
        REQUIRE(presence_computer != nullptr);
        REQUIRE(count_computer != nullptr);
        
        // Enable the computers
        presence_computer->setCheckState(1, Qt::Checked);
        count_computer->setCheckState(1, Qt::Checked);
        
        // Get enabled column infos
        auto column_infos = widget.getEnabledColumnInfos();
        
        REQUIRE(column_infos.size() == 2);
        
        // Verify column infos have correct properties
        bool has_presence_column = false;
        bool has_count_column = false;
        
        for (const auto& info : column_infos) {
            if (info.computerName.find("Event Presence") != std::string::npos) {
                has_presence_column = true;
                REQUIRE(info.outputTypeName == "bool");
                REQUIRE(!info.isVectorType);
            }
            if (info.computerName.find("Event Count") != std::string::npos) {
                has_count_column = true;
                REQUIRE(info.outputTypeName == "int");
                REQUIRE(!info.isVectorType);
            }
        }
        
        REQUIRE(has_presence_column);
        REQUIRE(has_count_column);
    }
    
    SECTION("Custom column names are preserved") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        auto* tree = widget.findChild<QTreeWidget*>("computers_tree");
        REQUIRE(tree != nullptr);
        
        // Find a computer item
        QTreeWidgetItem* computer_item = nullptr;
        for (int i = 0; i < tree->topLevelItemCount() && !computer_item; ++i) {
            auto* data_source_item = tree->topLevelItem(i);
            if (data_source_item->childCount() > 0) {
                computer_item = data_source_item->child(0);
            }
        }
        
        REQUIRE(computer_item != nullptr);
        
        // Set custom column name
        QString custom_name = "MyCustomColumnName";
        computer_item->setText(2, custom_name);
        computer_item->setCheckState(1, Qt::Checked);
        
        // Get enabled column infos
        auto column_infos = widget.getEnabledColumnInfos();
        
        REQUIRE(column_infos.size() == 1);
        REQUIRE(QString::fromStdString(column_infos[0].name) == custom_name);
    }
}
TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - JSON widget updates after enabling computer", "[TableDesignerWidget][JSON]") {
    TableDesignerWidget widget(getDataManagerPtr());

    // Create a table and select it
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("JsonTable");
    REQUIRE(registry.createTable(table_id, "JSON Table"));
    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) { table_combo->setCurrentIndex(i); break; }
    }

    // Select intervals row source
    auto * row_combo = widget.findChild<QComboBox*>("row_data_source_combo");
    REQUIRE(row_combo != nullptr);
    int interval_index = -1;
    for (int i = 0; i < row_combo->count(); ++i) {
        if (row_combo->itemText(i).contains("Intervals: BehaviorPeriods")) { interval_index = i; break; }
    }
    REQUIRE(interval_index >= 0);
    row_combo->setCurrentIndex(interval_index);

    // Enable one event computer under Neuron1Spikes
    auto * tree = widget.findChild<QTreeWidget*>("computers_tree");
    REQUIRE(tree != nullptr);
    QTreeWidgetItem * event_source_item = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto * item = tree->topLevelItem(i);
        if (item->text(0).contains("Events: Neuron1Spikes")) { event_source_item = item; break; }
    }
    REQUIRE(event_source_item != nullptr);
    QTreeWidgetItem * presence = nullptr;
    for (int j = 0; j < event_source_item->childCount(); ++j) {
        auto * c = event_source_item->child(j);
        if (c->text(0).contains("Event Presence")) { presence = c; break; }
    }
    REQUIRE(presence != nullptr);
    presence->setCheckState(1, Qt::Checked);

    // Build table which should populate JSON widget from current state
    REQUIRE(widget.buildTableFromTree());

    // Find JSON widget's editor by object name from ui
    auto * json_text = widget.findChild<QTextEdit*>("json_text_edit");
    REQUIRE(json_text != nullptr);
    auto text = json_text->toPlainText();
    REQUIRE(text.contains("\"columns\""));
    REQUIRE(text.contains("Neuron1Spikes"));
    REQUIRE(text.contains("Event Presence"));

    // Create a new empty table and populate it from the JSON text editor
    auto table_id2 = registry.generateUniqueTableId("JsonTable2");
    REQUIRE(registry.createTable(table_id2, "JSON Table 2"));

    // Select the new table
    table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id2)) { table_combo->setCurrentIndex(i); break; }
    }

    // Ensure row/computers cleared for new table
    auto * row_combo2 = widget.findChild<QComboBox*>("row_data_source_combo");
    REQUIRE(row_combo2 != nullptr);
    auto * tree2 = widget.findChild<QTreeWidget*>("computers_tree");
    REQUIRE(tree2 != nullptr);

    // Apply previously captured JSON to new table
    json_text->setPlainText(text);
    auto * apply_btn = widget.findChild<QPushButton*>("apply_json_btn");
    REQUIRE(apply_btn != nullptr);
    apply_btn->click();

    // Verify row selector got populated
    REQUIRE(row_combo2->currentText().contains("Intervals: BehaviorPeriods"));

    // Verify the expected computer is enabled under the data source
    QTreeWidgetItem * n1_item = nullptr;
    for (int i = 0; i < tree2->topLevelItemCount(); ++i) {
        auto * item = tree2->topLevelItem(i);
        if (item->text(0).contains("Events: Neuron1Spikes")) { n1_item = item; break; }
    }
    REQUIRE(n1_item != nullptr);
    bool presence_enabled = false;
    for (int j = 0; j < n1_item->childCount(); ++j) {
        auto * c = n1_item->child(j);
        if (c->text(0).contains("Event Presence")) {
            presence_enabled = (c->checkState(1) == Qt::Checked);
            break;
        }
    }
    REQUIRE(presence_enabled);

    // Verify enabled columns reflect JSON
    auto column_infos2 = widget.getEnabledColumnInfos();
    REQUIRE(column_infos2.size() >= 1);
    bool found_presence = false;
    for (auto const & ci : column_infos2) {
        if (ci.computerName.find("Event Presence") != std::string::npos && ci.dataSourceName.find("Neuron1Spikes") != std::string::npos) {
            found_presence = true;
            break;
        }
    }
    REQUIRE(found_presence);

    // Building from populated UI should succeed
    REQUIRE(widget.buildTableFromTree());

    // Now, save the JSON to a temporary file and load it using the widget's Load JSON button
    QTemporaryFile tmpFile(QDir::temp().filePath("table_json_XXXXXX.json"));
    REQUIRE(tmpFile.open());
    QByteArray jsonBytes = text.toUtf8();
    REQUIRE(tmpFile.write(jsonBytes) == jsonBytes.size());
    tmpFile.flush();

    // Create a third table and select it
    auto table_id3 = registry.generateUniqueTableId("JsonTable3");
    REQUIRE(registry.createTable(table_id3, "JSON Table 3"));
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id3)) { table_combo->setCurrentIndex(i); break; }
    }

    // Find the JSON widget's load button and force path
    auto * json_widget_load_btn = widget.findChild<QPushButton*>("load_json_btn");
    REQUIRE(json_widget_load_btn != nullptr);
    auto * json_widget = widget.findChild<TableJSONWidget*>();
    REQUIRE(json_widget != nullptr);
    json_widget->setForcedLoadPathForTests(tmpFile.fileName());

    // Click Load JSON to read from the temporary file
    json_widget_load_btn->click();

    // After loading, click Update Table to apply
    apply_btn->click();

    // Verify that the selection and computer enabling have been applied again
    REQUIRE(row_combo2->currentText().contains("Intervals: BehaviorPeriods"));
    n1_item = nullptr;
    for (int i = 0; i < tree2->topLevelItemCount(); ++i) {
        auto * item = tree2->topLevelItem(i);
        if (item->text(0).contains("Events: Neuron1Spikes")) { n1_item = item; break; }
    }
    REQUIRE(n1_item != nullptr);
    presence_enabled = false;
    for (int j = 0; j < n1_item->childCount(); ++j) {
        auto * c = n1_item->child(j);
        if (c->text(0).contains("Event Presence")) { presence_enabled = (c->checkState(1) == Qt::Checked); break; }
    }
    REQUIRE(presence_enabled);
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Invalid JSON shows error with line/column", "[TableDesignerWidget][JSON][Error]") {
    TableDesignerWidget widget(getDataManagerPtr());

    // Select a valid table to enable UI elements
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("InvalidJson");
    REQUIRE(registry.createTable(table_id, "Invalid JSON Test"));
    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) { table_combo->setCurrentIndex(i); break; }
    }

    // Provide invalid JSON with a known error position (missing comma)
    QString bad_json = R"({
  "tables": [
    {
      "table_id": "t1",
      "name": "Bad",
      "row_selector": { "type": "interval", "source": "BehaviorPeriods" }
      "columns": []
    }
  ]
})";

    // Put into editor
    auto * json_text = widget.findChild<QTextEdit*>("json_text_edit");
    REQUIRE(json_text != nullptr);
    json_text->setPlainText(bad_json);

    // Click Update Table to trigger parsing
    auto * apply_btn = widget.findChild<QPushButton*>("apply_json_btn");
    REQUIRE(apply_btn != nullptr);

    // Intercept message boxes by tracking active widgets before/after
    QList<QWidget*> before = QApplication::topLevelWidgets();
    apply_btn->click();
    QApplication::processEvents();
    QList<QWidget*> after = QApplication::topLevelWidgets();

    // Find a newly created QMessageBox with expected title/text
    QMessageBox * found = nullptr;
    for (QWidget * w : after) {
        if (!before.contains(w)) {
            auto * box = qobject_cast<QMessageBox*>(w);
            if (box && box->windowTitle() == "Invalid JSON") {
                found = box;
                break;
            }
        }
    }
    REQUIRE(found != nullptr);
    QString text = found->text();
    REQUIRE(text.contains("JSON format is invalid"));
    REQUIRE(text.contains("line"));
    REQUIRE(text.contains("column"));
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Valid JSON with missing keys reports errors", "[TableDesignerWidget][JSON][Error]") {
    TableDesignerWidget widget(getDataManagerPtr());
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("BadKeys");
    REQUIRE(registry.createTable(table_id, "Bad Keys"));
    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) { table_combo->setCurrentIndex(i); break; }
    }
    // Missing row_selector entirely
    QString json1 = R"({ "tables": [ { "columns": [ { "name": "c1", "data_source": "Neuron1Spikes", "computer": "Event Presence" } ] } ] })";
    auto * json_text = widget.findChild<QTextEdit*>("json_text_edit");
    REQUIRE(json_text != nullptr);
    json_text->setPlainText(json1);
    auto * apply_btn = widget.findChild<QPushButton*>("apply_json_btn");
    REQUIRE(apply_btn != nullptr);
    QList<QWidget*> before = QApplication::topLevelWidgets();
    apply_btn->click();
    QApplication::processEvents();
    QList<QWidget*> after = QApplication::topLevelWidgets();
    bool saw_error = false;
    for (QWidget * w : after) {
        if (!before.contains(w)) {
            if (auto * box = qobject_cast<QMessageBox*>(w)) {
                if (box->windowTitle() == "Invalid Table JSON" && box->text().contains("Missing required key: row_selector")) {
                    saw_error = true; break;
                }
            }
        }
    }
    REQUIRE(saw_error);
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Unknown computer reports error", "[TableDesignerWidget][JSON][Error]") {
    TableDesignerWidget widget(getDataManagerPtr());
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("BadComputer");
    REQUIRE(registry.createTable(table_id, "Bad Computer"));
    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) { table_combo->setCurrentIndex(i); break; }
    }
    QString json = R"({
      "tables": [
        {
          "row_selector": { "type": "interval", "source": "BehaviorPeriods" },
          "columns": [ { "name": "c1", "data_source": "Neuron1Spikes", "computer": "Does Not Exist" } ]
        }
      ]
    })";
    auto * json_text = widget.findChild<QTextEdit*>("json_text_edit");
    REQUIRE(json_text != nullptr);
    json_text->setPlainText(json);
    auto * apply_btn = widget.findChild<QPushButton*>("apply_json_btn");
    REQUIRE(apply_btn != nullptr);
    QList<QWidget*> before = QApplication::topLevelWidgets();
    apply_btn->click();
    QApplication::processEvents();
    QList<QWidget*> after = QApplication::topLevelWidgets();
    bool saw_error = false;
    for (QWidget * w : after) {
        if (!before.contains(w)) {
            if (auto * box = qobject_cast<QMessageBox*>(w)) {
                if (box->windowTitle() == "Invalid Table JSON" && box->text().contains("requested computer does not exist", Qt::CaseInsensitive)) { saw_error = true; break; }
            }
        }
    }
    REQUIRE(saw_error);
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Computer incompatible with data type reports error", "[TableDesignerWidget][JSON][Error]") {
    TableDesignerWidget widget(getDataManagerPtr());
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("BadCompat");
    REQUIRE(registry.createTable(table_id, "Bad Compat"));
    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) { table_combo->setCurrentIndex(i); break; }
    }
    // Use an analog-only computer on an Events source
    QString json = R"({
      "tables": [
        {
          "row_selector": { "type": "interval", "source": "BehaviorPeriods" },
          "columns": [ { "name": "c1", "data_source": "Neuron1Spikes", "computer": "Analog Mean" } ]
        }
      ]
    })";
    auto * json_text = widget.findChild<QTextEdit*>("json_text_edit");
    REQUIRE(json_text != nullptr);
    json_text->setPlainText(json);
    auto * apply_btn = widget.findChild<QPushButton*>("apply_json_btn");
    REQUIRE(apply_btn != nullptr);
    QList<QWidget*> before = QApplication::topLevelWidgets();
    apply_btn->click();
    QApplication::processEvents();
    QList<QWidget*> after = QApplication::topLevelWidgets();
    bool saw_error = false;
    for (QWidget * w : after) {
        if (!before.contains(w)) {
            if (auto * box = qobject_cast<QMessageBox*>(w)) {
                if (box->windowTitle() == "Invalid Table JSON" && box->text().contains("not valid for data source type", Qt::CaseInsensitive)) { saw_error = true; break; }
            }
        }
    }
    REQUIRE(saw_error);
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Data key not in DataManager reports error", "[TableDesignerWidget][JSON][Error]") {
    TableDesignerWidget widget(getDataManagerPtr());
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("BadDataKey");
    REQUIRE(registry.createTable(table_id, "Bad Data Key"));
    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) { table_combo->setCurrentIndex(i); break; }
    }
    QString json = R"({
      "tables": [
        {
          "row_selector": { "type": "interval", "source": "DoesNotExistIntervals" },
          "columns": [ { "name": "c1", "data_source": "DoesNotExistEvents", "computer": "Event Presence" } ]
        }
      ]
    })";
    auto * json_text = widget.findChild<QTextEdit*>("json_text_edit");
    REQUIRE(json_text != nullptr);
    json_text->setPlainText(json);
    auto * apply_btn = widget.findChild<QPushButton*>("apply_json_btn");
    REQUIRE(apply_btn != nullptr);
    QList<QWidget*> before = QApplication::topLevelWidgets();
    apply_btn->click();
    QApplication::processEvents();
    QList<QWidget*> after = QApplication::topLevelWidgets();
    bool saw_error = false;
    for (QWidget * w : after) {
        if (!before.contains(w)) {
            if (auto * box = qobject_cast<QMessageBox*>(w)) {
                if (box->windowTitle() == "Invalid Table JSON" && (box->text().contains("not found in DataManager", Qt::CaseInsensitive) || box->text().contains("Row selector data key not found", Qt::CaseInsensitive))) {
                    saw_error = true; break;
                }
            }
        }
    }
    REQUIRE(saw_error);
}


TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Table creation workflow", "[TableDesignerWidget][Workflow]") {
    
    SECTION("Complete workflow - create table, enable computers, build table") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        // Create a new table first
        auto& registry = getTableRegistry();
        auto table_id = registry.generateUniqueTableId("TestTable");
        REQUIRE(registry.createTable(table_id, "Test Table for Workflow"));
        
        // Set the table in the widget (simulate user selection)
        auto* table_combo = widget.findChild<QComboBox*>("table_combo");
        REQUIRE(table_combo != nullptr);
        
        // Find the table in the combo and select it
        for (int i = 0; i < table_combo->count(); ++i) {
            if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) {
                table_combo->setCurrentIndex(i);
                break;
            }
        }
        
        // Set row data source (simulate user selection)
        auto* row_combo = widget.findChild<QComboBox*>("row_data_source_combo");
        REQUIRE(row_combo != nullptr);
        
        // Find and select an interval source for rows
        for (int i = 0; i < row_combo->count(); ++i) {
            if (row_combo->itemText(i).contains("Intervals: BehaviorPeriods")) {
                row_combo->setCurrentIndex(i);
                break;
            }
        }
        
        // Enable some computers in the tree
        auto* tree = widget.findChild<QTreeWidget*>("computers_tree");
        REQUIRE(tree != nullptr);
        
        int enabled_computers = 0;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto* data_source_item = tree->topLevelItem(i);
            
            for (int j = 0; j < data_source_item->childCount() && enabled_computers < 3; ++j) {
                auto* computer_item = data_source_item->child(j);
                computer_item->setCheckState(1, Qt::Checked);
                enabled_computers++;
            }
        }
        
        REQUIRE(enabled_computers > 0);
        
        // Verify column infos are generated
        auto column_infos = widget.getEnabledColumnInfos();
        REQUIRE(column_infos.size() == enabled_computers);
        
        // Build the table
        bool build_success = widget.buildTableFromTree();
        
        // Verify the table was built and stored
        auto built_table = registry.getBuiltTable(table_id);
        REQUIRE(built_table != nullptr);
        REQUIRE(built_table->getColumnCount() == column_infos.size());
    }
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Preview updates when columns enabled", "[TableDesignerWidget][Preview]") {
    TableDesignerWidget widget(getDataManagerPtr());

    // Create a new table and select it
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("PreviewTable");
    REQUIRE(registry.createTable(table_id, "Preview Table"));

    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) {
            table_combo->setCurrentIndex(i);
            break;
        }
    }

    // Select intervals row source
    auto * row_combo = widget.findChild<QComboBox*>("row_data_source_combo");
    REQUIRE(row_combo != nullptr);
    int interval_index = -1;
    for (int i = 0; i < row_combo->count(); ++i) {
        if (row_combo->itemText(i).contains("Intervals: BehaviorPeriods")) { interval_index = i; break; }
    }
    REQUIRE(interval_index >= 0);
    row_combo->setCurrentIndex(interval_index);

    // Enable two event computers under Neuron1Spikes
    auto * tree = widget.findChild<QTreeWidget*>("computers_tree");
    REQUIRE(tree != nullptr);

    QTreeWidgetItem * event_source_item = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto * item = tree->topLevelItem(i);
        if (item->text(0).contains("Events: Neuron1Spikes")) { event_source_item = item; break; }
    }
    REQUIRE(event_source_item != nullptr);
    REQUIRE(event_source_item->childCount() > 0);

    QTreeWidgetItem * presence_computer = nullptr;
    QTreeWidgetItem * count_computer = nullptr;
    for (int j = 0; j < event_source_item->childCount(); ++j) {
        auto * computer_item = event_source_item->child(j);
        auto name = computer_item->text(0);
        if (name.contains("Event Presence")) presence_computer = computer_item;
        if (name.contains("Event Count")) count_computer = computer_item;
    }
    REQUIRE(presence_computer != nullptr);
    REQUIRE(count_computer != nullptr);

    presence_computer->setCheckState(1, Qt::Checked);
    count_computer->setCheckState(1, Qt::Checked);

    // Find the embedded TableView and its model
    auto * table_view_widget = widget.findChild<QTableView*>();
    REQUIRE(table_view_widget != nullptr);
    auto * model = table_view_widget->model();
    REQUIRE(model != nullptr);

    // Expect 4 behavior intervals as rows, and 2 enabled columns
    REQUIRE(model->rowCount() == 4);
    REQUIRE(model->columnCount() == 2);
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Observes DataManager and updates tree on add/remove", "[TableDesignerWidget][Observer]") {
    TableDesignerWidget widget(getDataManagerPtr());

    auto * tree = widget.findChild<QTreeWidget*>("computers_tree");
    REQUIRE(tree != nullptr);

    // Initially, no "Events: NewSpikes" source
    bool found_new = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0).contains("Events: NewSpikes")) { found_new = true; break; }
    }
    REQUIRE(!found_new);

    // Enable a computer under existing Neuron1Spikes to test state preservation
    QTreeWidgetItem * n1_item = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto * item = tree->topLevelItem(i);
        if (item->text(0).contains("Events: Neuron1Spikes")) { n1_item = item; break; }
    }
    REQUIRE(n1_item != nullptr);
    QTreeWidgetItem * presence = nullptr;
    for (int j = 0; j < n1_item->childCount(); ++j) {
        auto * c = n1_item->child(j);
        if (c->text(0).contains("Event Presence")) { presence = c; break; }
    }
    REQUIRE(presence != nullptr);
    presence->setCheckState(1, Qt::Checked);

    // Add a new event key to DataManager and ensure tree updates
    auto & dm = getDataManager();
    std::vector<float> spikes = {1.0f, 2.0f, 3.0f};
    auto series = std::make_shared<DigitalEventSeries>(spikes);
    dm.setData<DigitalEventSeries>("NewSpikes", series, TimeKey("spike_time"));

    // Process events to allow observer callback to run
    QApplication::processEvents();

    // Tree should now include the new source
    // Refresh pointer (not strictly necessary but clearer)
    tree = widget.findChild<QTreeWidget*>("computers_tree");
    REQUIRE(tree != nullptr);
    bool found_after = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0).contains("Events: NewSpikes")) { found_after = true; break; }
    }
    REQUIRE(found_after);

    // State for previously enabled computer should be preserved
    // Re-find Neuron1Spikes item
    n1_item = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto * item = tree->topLevelItem(i);
        if (item->text(0).contains("Events: Neuron1Spikes")) { n1_item = item; break; }
    }
    REQUIRE(n1_item != nullptr);
    bool still_checked = false;
    for (int j = 0; j < n1_item->childCount(); ++j) {
        auto * c = n1_item->child(j);
        if (c->text(0).contains("Event Presence")) { still_checked = (c->checkState(1) == Qt::Checked); break; }
    }
    REQUIRE(still_checked);

    // Enable a computer under the new source to test removal cleanup
    QTreeWidgetItem * new_item = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto * item = tree->topLevelItem(i);
        if (item->text(0).contains("Events: NewSpikes")) { new_item = item; break; }
    }
    REQUIRE(new_item != nullptr);
    int new_checked_before = 0;
    for (int j = 0; j < new_item->childCount(); ++j) {
        auto * c = new_item->child(j);
        if (c->text(0).contains("Event Presence") || c->text(0).contains("Event Count")) {
            c->setCheckState(1, Qt::Checked);
            ++new_checked_before;
            break; // enable one
        }
    }
    REQUIRE(new_checked_before >= 1);

    // Now remove the new data source and ensure its entry disappears
    REQUIRE(dm.deleteData("NewSpikes"));
    QApplication::processEvents();

    bool removed = true;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0).contains("Events: NewSpikes")) { removed = false; break; }
    }
    REQUIRE(removed);

    // Also verify enabled columns no longer include NewSpikes
    auto enabled_columns = widget.getEnabledColumnInfos();
    for (auto const & info : enabled_columns) {
        REQUIRE(std::string(info.dataSourceName).find("NewSpikes") == std::string::npos);
    }
}

TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Drag-reorder columns updates visual order", "[TableDesignerWidget][Reorder]") {
    TableDesignerWidget widget(getDataManagerPtr());

    // Create a table and select it
    auto & registry = getTableRegistry();
    auto table_id = registry.generateUniqueTableId("Reorder");
    REQUIRE(registry.createTable(table_id, "Reorder Table"));
    auto * table_combo = widget.findChild<QComboBox*>("table_combo");
    REQUIRE(table_combo != nullptr);
    for (int i = 0; i < table_combo->count(); ++i) {
        if (table_combo->itemData(i).toString() == QString::fromStdString(table_id)) { table_combo->setCurrentIndex(i); break; }
    }

    // Select intervals as rows and enable two computers under Neuron1Spikes
    auto * row_combo = widget.findChild<QComboBox*>("row_data_source_combo");
    REQUIRE(row_combo != nullptr);
    for (int i = 0; i < row_combo->count(); ++i) {
        if (row_combo->itemText(i).contains("Intervals: BehaviorPeriods")) { row_combo->setCurrentIndex(i); break; }
    }
    auto * tree = widget.findChild<QTreeWidget*>("computers_tree");
    REQUIRE(tree != nullptr);
    QTreeWidgetItem * n1 = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0).contains("Events: Neuron1Spikes")) { n1 = tree->topLevelItem(i); break; }
    }
    REQUIRE(n1 != nullptr);
    QTreeWidgetItem * presence = nullptr; QTreeWidgetItem * count = nullptr;
    for (int j = 0; j < n1->childCount(); ++j) {
        auto * c = n1->child(j);
        if (c->text(0).contains("Event Presence")) presence = c;
        if (c->text(0).contains("Event Count")) count = c;
    }
    REQUIRE(presence != nullptr);
    REQUIRE(count != nullptr);
    presence->setCheckState(1, Qt::Checked);
    count->setCheckState(1, Qt::Checked);

    // Get the embedded preview table and reorder columns visually
    auto * tv = widget.findChild<QTableView*>();
    REQUIRE(tv != nullptr);
    auto * header = tv->horizontalHeader();
    REQUIRE(header != nullptr);

    // Expect initial order [Presence, Count] (order determined by tree population)
    int col0 = header->logicalIndex(0);
    int col1 = header->logicalIndex(1);
    REQUIRE(col0 != col1);

    // Move second column to the first position
    header->moveSection(1, 0);
    QApplication::processEvents();

    // Verify visual order changed
    REQUIRE(header->visualIndex(col1) == 0);
    REQUIRE(header->visualIndex(col0) == 1);
}

/*
TEST_CASE_METHOD(TableDesignerWidgetTestFixture, "TableDesignerWidget - Data source compatibility", "[TableDesignerWidget][Compatibility]") {
    
    SECTION("Event sources should have event computers") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        // Test computer compatibility
        REQUIRE(widget.isComputerCompatibleWithDataSource("Event Presence", "Events: Neuron1Spikes"));
        REQUIRE(widget.isComputerCompatibleWithDataSource("Event Count", "Events: Neuron1Spikes"));
        REQUIRE(widget.isComputerCompatibleWithDataSource("Event Gather", "Events: Neuron1Spikes"));
        
        // Event computers should not be compatible with analog sources
        REQUIRE(!widget.isComputerCompatibleWithDataSource("Event Presence", "analog:LFP"));
        REQUIRE(!widget.isComputerCompatibleWithDataSource("Event Count", "analog:LFP"));
    }
    
    SECTION("Analog sources should have analog computers") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        // Test analog computer compatibility
        REQUIRE(widget.isComputerCompatibleWithDataSource("Analog Slice Gatherer", "analog:LFP"));
        REQUIRE(widget.isComputerCompatibleWithDataSource("Analog Mean", "analog:LFP"));
        REQUIRE(widget.isComputerCompatibleWithDataSource("Analog Max", "analog:EMG"));
        
        // Analog computers should not be compatible with event sources
        REQUIRE(!widget.isComputerCompatibleWithDataSource("Analog Mean", "Events: Neuron1Spikes"));
        REQUIRE(!widget.isComputerCompatibleWithDataSource("Analog Slice Gatherer", "Events: Neuron1Spikes"));
    }
    
    SECTION("Default column name generation") {
        TableDesignerWidget widget(getDataManagerPtr());
        
        // Test default name generation
        QString name1 = widget.generateDefaultColumnName("Events: Neuron1Spikes", "Event Presence");
        QString name2 = widget.generateDefaultColumnName("analog:LFP", "Analog Mean");
        QString name3 = widget.generateDefaultColumnName("Intervals: BehaviorPeriods", "Event Count");
        
        REQUIRE(name1 == "Neuron1Spikes_Event Presence");
        REQUIRE(name2 == "LFP_Analog Mean");
        REQUIRE(name3 == "BehaviorPeriods_Event Count");
        
        // Names should be unique for different combinations
        REQUIRE(name1 != name2);
        REQUIRE(name2 != name3);
        REQUIRE(name1 != name3);
    }
}
*/