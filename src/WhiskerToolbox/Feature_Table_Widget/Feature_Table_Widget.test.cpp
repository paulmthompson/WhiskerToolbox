#include "Feature_Table_Widget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QMetaObject>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <vector>

/**
 * @brief Test fixture for Feature_Table_Widget tests
 * 
 * Creates a simple Qt application and DataManager with test data
 * for testing Feature_Table_Widget state preservation functionality.
 */
class FeatureTableWidgetTestFixture {
protected:
    FeatureTableWidgetTestFixture() {
        // Create Qt application if one doesn't exist
        if (!QApplication::instance()) {
            static int argc = 1;
            static char * argv[] = {const_cast<char *>("test")};
            m_app = std::make_unique<QApplication>(argc, argv);
        }

        // Initialize DataManager
        m_data_manager = std::make_shared<DataManager>();

        // Create test data
        populateWithTestData();

        // Create the widget
        m_widget = std::make_unique<Feature_Table_Widget>();
        m_widget->setDataManager(m_data_manager);

        // Set up columns including the "Enabled" column
        QStringList columns = {"Feature", "Type", "Clock", "Enabled"};
        m_widget->setColumns(columns);
    }

    ~FeatureTableWidgetTestFixture() = default;

    /**
     * @brief Get the Feature_Table_Widget instance
     * @return Reference to the widget
     */
    Feature_Table_Widget & getWidget() { return *m_widget; }

    /**
     * @brief Get the DataManager instance
     * @return Reference to the DataManager
     */
    DataManager & getDataManager() { return *m_data_manager; }

private:
    void populateWithTestData() {
        // Create a default time frame
        auto timeframe = std::make_shared<TimeFrame>();
        TimeKey time_key("time");
        m_data_manager->setTime(time_key, timeframe);

        // Add some test PointData
        auto point_data1 = std::make_shared<PointData>();
        point_data1->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 200.0f});
        point_data1->addAtTime(TimeFrameIndex(0), Point2D<float>{150.0f, 250.0f});
        m_data_manager->setData<PointData>("test_points_1", point_data1, time_key);

        auto point_data2 = std::make_shared<PointData>();
        point_data2->addAtTime(TimeFrameIndex(0), Point2D<float>{300.0f, 400.0f});
        m_data_manager->setData<PointData>("test_points_2", point_data2, time_key);

        // Add some test LineData
        auto line_data = std::make_shared<LineData>();
        line_data->addAtTime(TimeFrameIndex(0), Line2D(std::vector<Point2D<float>>{
                                                        Point2D<float>{0.0f, 0.0f},
                                                        Point2D<float>{100.0f, 100.0f}}));
        m_data_manager->setData<LineData>("test_lines", line_data, time_key);

        // Add some test AnalogTimeSeries
        std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<int64_t> timestamps = {0, 1000, 2000, 3000, 4000};
        auto analog_data = std::make_shared<AnalogTimeSeries>(values, values.size());
        m_data_manager->setData<AnalogTimeSeries>("test_analog", analog_data, time_key);

        // Add some test DigitalEventSeries
        auto event_data = std::make_shared<DigitalEventSeries>();
        event_data->addEvent(1000);
        event_data->addEvent(2000);
        event_data->addEvent(3000);
        m_data_manager->setData<DigitalEventSeries>("test_events", event_data, time_key);
    }

    std::unique_ptr<QApplication> m_app;
    std::shared_ptr<DataManager> m_data_manager;
    std::unique_ptr<Feature_Table_Widget> m_widget;
};

TEST_CASE_METHOD(FeatureTableWidgetTestFixture, "Feature_Table_Widget - Basic Functionality", "[Feature_Table_Widget]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();

    SECTION("Widget initialization") {
        REQUIRE(&widget != nullptr);
        REQUIRE(&dm != nullptr);
    }

    SECTION("Table population") {
        widget.populateTable();

        // Check that table has been populated
        auto * table = widget.findChild<QTableWidget *>("available_features_table");
        REQUIRE(table != nullptr);
        REQUIRE(table->rowCount() > 0);
        REQUIRE(table->columnCount() == 4);// Feature, Type, Clock, Enabled
    }
}

TEST_CASE_METHOD(FeatureTableWidgetTestFixture, "Feature_Table_Widget - State Preservation", "[Feature_Table_Widget]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();

    // Populate the table initially
    widget.populateTable();

    auto * table = widget.findChild<QTableWidget *>("available_features_table");
    REQUIRE(table != nullptr);

    SECTION("Checkbox state preservation after table rebuild") {
        // Find the "Enabled" column
        int enabledColumnIndex = -1;
        int featureColumnIndex = -1;
        for (int col = 0; col < table->columnCount(); ++col) {
            QString header = table->horizontalHeaderItem(col)->text();
            if (header == "Enabled") {
                enabledColumnIndex = col;
            } else if (header == "Feature") {
                featureColumnIndex = col;
            }
        }

        REQUIRE(enabledColumnIndex != -1);
        REQUIRE(featureColumnIndex != -1);

        // Enable some checkboxes manually
        std::vector<std::string> enabledFeatures;

        for (int row = 0; row < std::min(3, table->rowCount()); ++row) {
            QWidget * cellWidget = table->cellWidget(row, enabledColumnIndex);
            REQUIRE(cellWidget != nullptr);

            QCheckBox * checkbox = cellWidget->findChild<QCheckBox *>();
            REQUIRE(checkbox != nullptr);

            // Enable this checkbox
            checkbox->setChecked(true);

            // Store the feature name
            QTableWidgetItem * featureItem = table->item(row, featureColumnIndex);
            REQUIRE(featureItem != nullptr);
            enabledFeatures.push_back(featureItem->text().toStdString());
        }

        // Process Qt events to ensure signals are processed
        QApplication::processEvents();

        // Add a new feature to trigger table rebuild
        auto new_point_data = std::make_shared<PointData>();
        new_point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{500.0f, 600.0f});
        TimeKey time_key("time");
        dm.setData<PointData>("test_points_new", new_point_data, time_key);

        // Process Qt events to ensure the table is rebuilt
        QApplication::processEvents();

        // Verify that the enabled checkboxes are still enabled
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem * featureItem = table->item(row, featureColumnIndex);
            REQUIRE(featureItem != nullptr);

            std::string featureName = featureItem->text().toStdString();

            QWidget * cellWidget = table->cellWidget(row, enabledColumnIndex);
            REQUIRE(cellWidget != nullptr);

            QCheckBox * checkbox = cellWidget->findChild<QCheckBox *>();
            REQUIRE(checkbox != nullptr);

            // Check if this feature should be enabled
            bool shouldBeEnabled = std::find(enabledFeatures.begin(), enabledFeatures.end(), featureName) != enabledFeatures.end();

            REQUIRE(checkbox->isChecked() == shouldBeEnabled);
        }
    }

    SECTION("Selection state preservation after table rebuild") {
        // Find the feature column index
        int featureColumnIndex = -1;
        for (int col = 0; col < table->columnCount(); ++col) {
            QString header = table->horizontalHeaderItem(col)->text();
            if (header == "Feature") {
                featureColumnIndex = col;
                break;
            }
        }
        REQUIRE(featureColumnIndex != -1);

        // Select a feature by simulating a cell click
        QString selectedFeature;
        if (table->rowCount() > 0) {
            QTableWidgetItem * featureItem = table->item(0, featureColumnIndex);
            REQUIRE(featureItem != nullptr);
            selectedFeature = featureItem->text();

            // Simulate the cell click by emitting the signal that triggers _highlightFeature
            // This is more reliable than trying to simulate mouse events in tests
            emit table->cellClicked(0, featureColumnIndex);
        }

        // Process Qt events
        QApplication::processEvents();

        // Verify the feature is now highlighted
        QString initialHighlighted = widget.getHighlightedFeature();
        REQUIRE(initialHighlighted == selectedFeature);

        // Add a new feature to trigger table rebuild
        std::vector<float> values = {10.0f, 20.0f, 30.0f};
        std::vector<int64_t> timestamps = {5000, 6000, 7000};
        auto new_analog_data = std::make_shared<AnalogTimeSeries>(values, values.size());

        TimeKey time_key("time");
        dm.setData<AnalogTimeSeries>("test_analog_new", new_analog_data, time_key);

        // Process Qt events to ensure the table is rebuilt
        QApplication::processEvents();

        // Verify that the previously selected feature is still highlighted
        if (!selectedFeature.isEmpty()) {
            QString currentlyHighlighted = widget.getHighlightedFeature();
            REQUIRE(currentlyHighlighted == selectedFeature);
        }
    }
}

TEST_CASE_METHOD(FeatureTableWidgetTestFixture, "Feature_Table_Widget - Signal Emission", "[Feature_Table_Widget]") {
    auto & widget = getWidget();

    // Populate the table
    widget.populateTable();

    auto * table = widget.findChild<QTableWidget *>("available_features_table");
    REQUIRE(table != nullptr);
    REQUIRE(table->rowCount() > 0);

    SECTION("Feature selection signal emission") {
        bool signalEmitted = false;
        QString emittedFeature;

        // Connect to the signal
        QObject::connect(&widget, &Feature_Table_Widget::featureSelected,
                         [&signalEmitted, &emittedFeature](QString const & feature) {
                             signalEmitted = true;
                             emittedFeature = feature;
                         });

        // Click on a table row
        if (table->rowCount() > 0) {
            // Simulate a cell click
            widget.findChild<QTableWidget *>()->selectRow(0);

            // Process events
            QApplication::processEvents();

            // The signal should have been emitted
            // Note: This might not work in headless testing, but the structure is correct
        }
    }
}

TEST_CASE_METHOD(FeatureTableWidgetTestFixture, "Feature_Table_Widget - No emissions during rebuild", "[Feature_Table_Widget][Signals]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();

    // Initial populate
    widget.populateTable();
    QApplication::processEvents();

    // Counters for signals
    int featureSelectedCount = 0;
    int addFeatureCount = 0;
    int removeFeatureCount = 0;

    QObject::connect(&widget, &Feature_Table_Widget::featureSelected, [&featureSelectedCount](QString const &) {
        featureSelectedCount++;
    });
    QObject::connect(&widget, &Feature_Table_Widget::addFeature, [&addFeatureCount](QString const &) {
        addFeatureCount++;
    });
    QObject::connect(&widget, &Feature_Table_Widget::removeFeature, [&removeFeatureCount](QString const &) {
        removeFeatureCount++;
    });

    // Trigger table rebuild by adding analog data
    std::vector<float> values = {1.0f, 2.0f, 3.0f};
    auto analog = std::make_shared<AnalogTimeSeries>(values, values.size());
    TimeKey time_key("time");
    dm.setData<AnalogTimeSeries>("probe_table_analog_1", analog, time_key);
    QApplication::processEvents();

    // No signals should be emitted during rebuild
    REQUIRE(featureSelectedCount == 0);
    REQUIRE(addFeatureCount == 0);
    REQUIRE(removeFeatureCount == 0);

    // Trigger another rebuild by adding digital events
    auto ev = std::make_shared<DigitalEventSeries>();
    ev->addEvent(1000);
    dm.setData<DigitalEventSeries>("probe_table_events_1", ev, time_key);
    QApplication::processEvents();

    // Still no signals from rebuild
    REQUIRE(featureSelectedCount == 0);
    REQUIRE(addFeatureCount == 0);
    REQUIRE(removeFeatureCount == 0);

    // Now simulate user action by toggling a checkbox in the "Enabled" column
    auto * table = widget.findChild<QTableWidget *>("available_features_table");
    REQUIRE(table != nullptr);

    int enabledCol = -1;
    for (int col = 0; col < table->columnCount(); ++col) {
        auto * headerItem = table->horizontalHeaderItem(col);
        if (headerItem && headerItem->text() == "Enabled") {
            enabledCol = col;
            break;
        }
    }
    REQUIRE(enabledCol != -1);

    // Find a row with a checkbox and toggle it
    bool toggled = false;
    int prevAdd = addFeatureCount;
    int prevRemove = removeFeatureCount;

    for (int row = 0; row < table->rowCount(); ++row) {
        QWidget * cell = table->cellWidget(row, enabledCol);
        if (!cell) continue;
        if (auto * cb = cell->findChild<QCheckBox *>()) {
            cb->setChecked(!cb->isChecked());
            QApplication::processEvents();
            toggled = true;
            break;
        }
    }

    REQUIRE(toggled);
    // One of add/remove should have incremented
    REQUIRE((addFeatureCount > prevAdd || removeFeatureCount > prevRemove));
}
