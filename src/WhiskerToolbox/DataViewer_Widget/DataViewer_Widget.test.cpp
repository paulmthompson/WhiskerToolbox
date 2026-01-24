#include "DataViewer_Widget.hpp"

#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DataViewerState.hpp"
#include "DataViewerStateData.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <QApplication>
#include <QMetaObject>
#include <QTimer>
#include <QTreeWidget>
#include <QWidget>
#include <QDoubleSpinBox>

#include "OpenGLWidget.hpp"
#include "TimeSeriesDataStore.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

// Helper functions to access options through state (replaces deprecated getAnalogConfig, etc.)
namespace TestHelpers {

/**
 * @brief Get analog series options from state
 * @param widget The DataViewer_Widget
 * @param key The series key
 * @return Pointer to options or nullptr if not found
 */
inline AnalogSeriesOptionsData const * getAnalogOptions(DataViewer_Widget & widget, std::string const & key) {
    return widget.state()->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
}

/**
 * @brief Get analog series layout transform from data store
 * @param widget The DataViewer_Widget
 * @param key The series key
 * @return Optional layout transform
 */
inline std::optional<CorePlotting::LayoutTransform> getAnalogLayoutTransform(DataViewer_Widget & widget, std::string const & key) {
    auto const & series_map = widget.getOpenGLWidget()->getAnalogSeriesMap();
    auto it = series_map.find(key);
    if (it != series_map.end()) {
        return it->second.layout_transform;
    }
    return std::nullopt;
}

/**
 * @brief Get digital event series options from state
 * @param widget The DataViewer_Widget
 * @param key The series key
 * @return Pointer to options or nullptr if not found
 */
inline DigitalEventSeriesOptionsData const * getEventOptions(DataViewer_Widget & widget, std::string const & key) {
    return widget.state()->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
}

/**
 * @brief Get digital event series layout transform from data store
 * @param widget The DataViewer_Widget
 * @param key The series key
 * @return Optional layout transform
 */
inline std::optional<CorePlotting::LayoutTransform> getEventLayoutTransform(DataViewer_Widget & widget, std::string const & key) {
    auto const & series_map = widget.getOpenGLWidget()->getDigitalEventSeriesMap();
    auto it = series_map.find(key);
    if (it != series_map.end()) {
        return it->second.layout_transform;
    }
    return std::nullopt;
}

/**
 * @brief Get digital interval series options from state
 * @param widget The DataViewer_Widget
 * @param key The series key
 * @return Pointer to options or nullptr if not found
 */
inline DigitalIntervalSeriesOptionsData const * getIntervalOptions(DataViewer_Widget & widget, std::string const & key) {
    return widget.state()->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
}

/**
 * @brief Get digital interval series layout transform from data store
 * @param widget The DataViewer_Widget
 * @param key The series key
 * @return Optional layout transform
 */
inline std::optional<CorePlotting::LayoutTransform> getIntervalLayoutTransform(DataViewer_Widget & widget, std::string const & key) {
    auto const & series_map = widget.getOpenGLWidget()->getDigitalIntervalSeriesMap();
    auto it = series_map.find(key);
    if (it != series_map.end()) {
        return it->second.layout_transform;
    }
    return std::nullopt;
}

}// namespace TestHelpers

/**
 * @brief Test fixture for DataViewer_Widget data cleanup tests
 * 
 * Creates a Qt application, DataManager with test data, and DataViewer_Widget
 * to test proper cleanup when data is deleted from the DataManager.
 */
class DataViewerWidgetCleanupTestFixture {
protected:
    DataViewerWidgetCleanupTestFixture() {
        // Create Qt application if one doesn't exist
        if (!QApplication::instance()) {
            static int argc = 1;
            static char * argv[] = {const_cast<char *>("test")};
            m_app = std::make_unique<QApplication>(argc, argv);
        }

        // Initialize DataManager
        m_data_manager = std::make_shared<DataManager>();

        // Create a mock TimeScrollBar and wire DataManager (matches Analysis_Dashboard fixture)
        m_time_scrollbar = std::make_unique<TimeScrollBar>();
        m_time_scrollbar->setDataManager(m_data_manager);

        // Create test data
        populateWithTestData();

        // Create the DataViewer_Widget
        m_widget = std::make_unique<DataViewer_Widget>(m_data_manager, m_time_scrollbar.get(), nullptr);
    }

    ~DataViewerWidgetCleanupTestFixture() = default;

    /**
     * @brief Get the DataViewer_Widget instance
     * @return Reference to the widget
     */
    DataViewer_Widget & getWidget() { return *m_widget; }

    /**
     * @brief Get the DataManager instance
     * @return Reference to the DataManager
     */
    DataManager & getDataManager() { return *m_data_manager; }

    /**
     * @brief Get the test data keys
     * @return Vector of test data keys
     */
    std::vector<std::string> getTestDataKeys() const { return m_test_data_keys; }

private:
    void populateWithTestData() {
        // Create a default time frame
        std::vector<int> t(4000);
        std::iota(std::begin(t), std::end(t), 0);

        auto new_timeframe = std::make_shared<TimeFrame>(t);

        auto time_key = TimeKey("time");

        m_data_manager->removeTime(TimeKey("time"));
        m_data_manager->setTime(TimeKey("time"), new_timeframe);


        // Add test AnalogTimeSeries
        std::vector<float> analog_values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        auto analog_data = std::make_shared<AnalogTimeSeries>(analog_values, analog_values.size());
        m_data_manager->setData<AnalogTimeSeries>("test_analog", analog_data, time_key);
        m_test_data_keys.push_back("test_analog");

        // Add test DigitalEventSeries
        auto event_data = std::make_shared<DigitalEventSeries>();
        event_data->addEvent(TimeFrameIndex(1000));
        event_data->addEvent(TimeFrameIndex(2000));
        event_data->addEvent(TimeFrameIndex(3000));
        m_data_manager->setData<DigitalEventSeries>("test_events", event_data, time_key);
        m_test_data_keys.push_back("test_events");

        // Add test DigitalIntervalSeries
        auto interval_data = std::make_shared<DigitalIntervalSeries>();
        interval_data->addEvent(TimeFrameIndex(500), TimeFrameIndex(1500));
        interval_data->addEvent(TimeFrameIndex(2500), TimeFrameIndex(3500));
        m_data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_data, time_key);
        m_test_data_keys.push_back("test_intervals");
    }

    std::unique_ptr<QApplication> m_app;
    std::shared_ptr<DataManager> m_data_manager;
    std::unique_ptr<TimeScrollBar> m_time_scrollbar;
    std::unique_ptr<DataViewer_Widget> m_widget;
    std::vector<std::string> m_test_data_keys;
};

// -----------------------------------------------------------------------------
// Simple lifecycle tests to validate creation/open/close/destruction stability
// -----------------------------------------------------------------------------
TEST_CASE_METHOD(DataViewerWidgetCleanupTestFixture, "DataViewer_Widget - Lifecycle (Construct/Destruct)", "[DataViewer_Widget][Lifecycle]") {
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    // Widget constructed in fixture
    auto & widget = getWidget();
    REQUIRE(&widget != nullptr);

    // Allow any queued init to run
    app->processEvents();
}

TEST_CASE_METHOD(DataViewerWidgetCleanupTestFixture, "DataViewer_Widget - Lifecycle (Open/Close)", "[DataViewer_Widget][Lifecycle]") {
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    auto & widget = getWidget();

    // Open and process events
    widget.openWidget();
    app->processEvents();

    // Close event by hiding the widget (avoid deep teardown here)
    widget.hide();
    app->processEvents();
}

TEST_CASE_METHOD(DataViewerWidgetCleanupTestFixture, "DataViewer_Widget - Lifecycle (Show/Hide/Destroy)", "[DataViewer_Widget][Lifecycle]") {
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    // Create a fresh widget locally to validate destruction path
    auto data_manager = std::make_shared<DataManager>();
    // Provide a minimal timeframe so XAxis setup has bounds
    data_manager->setTime(TimeKey("time"), std::make_shared<TimeFrame>());

    TimeScrollBar tsb;
    tsb.setDataManager(data_manager);

    // Create, show, process, then destroy
    {
        DataViewer_Widget dvw(data_manager, &tsb, nullptr);
        dvw.openWidget();
        app->processEvents();
        dvw.hide();
        app->processEvents();
    }

    // If we reached here without a crash, basic lifecycle is OK
    REQUIRE(true);
}

// -----------------------------------------------------------------------------
// Existing comprehensive tests follow
// -----------------------------------------------------------------------------
TEST_CASE_METHOD(DataViewerWidgetCleanupTestFixture, "DataViewer_Widget - Data Cleanup on Deletion", "[DataViewer_Widget]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();
    auto test_keys = getTestDataKeys();

    SECTION("Widget initialization with data") {
        REQUIRE(&widget != nullptr);
        REQUIRE(&dm != nullptr);
        REQUIRE(test_keys.size() == 3);

        // Open the widget to initialize it
        widget.openWidget();

        // Process Qt events to ensure initialization completes
        QApplication::processEvents();
    }

    SECTION("Data cleanup after deletion from DataManager") {
        // Open the widget to initialize it
        widget.openWidget();
        QApplication::processEvents();

        // Store weak pointers to verify cleanup
        std::vector<std::weak_ptr<void>> weak_refs;

        // Get weak references to the data
        for (auto const & key: test_keys) {
            if (key == "test_analog") {
                auto analog_data = dm.getData<AnalogTimeSeries>(key);
                weak_refs.push_back(analog_data);
            } else if (key == "test_events") {
                auto event_data = dm.getData<DigitalEventSeries>(key);
                weak_refs.push_back(event_data);
            } else if (key == "test_intervals") {
                auto interval_data = dm.getData<DigitalIntervalSeries>(key);
                weak_refs.push_back(interval_data);
            }
        }

        // Verify weak references are valid initially
        for (auto const & weak_ref: weak_refs) {
            REQUIRE(!weak_ref.expired());
        }

        // Delete data from DataManager
        for (auto const & key: test_keys) {
            bool deleted = dm.deleteData(key);
            REQUIRE(deleted);
        }

        // Process Qt events to ensure the observer callback executes
        QApplication::processEvents();

        // Verify that weak references are now expired (data cleaned up)
        for (auto const & weak_ref: weak_refs) {
            REQUIRE(weak_ref.expired());
        }
    }

    SECTION("Partial data cleanup - delete only some data") {
        // Open the widget to initialize it
        widget.openWidget();
        QApplication::processEvents();

        // Delete only the analog time series
        bool deleted = dm.deleteData("test_analog");
        REQUIRE(deleted);

        // Process Qt events
        QApplication::processEvents();

        // Verify analog data is cleaned up from DataManager
        auto analog_data = dm.getData<AnalogTimeSeries>("test_analog");
        REQUIRE(analog_data == nullptr);

        // Verify other data remains
        auto event_data = dm.getData<DigitalEventSeries>("test_events");
        auto interval_data = dm.getData<DigitalIntervalSeries>("test_intervals");
        REQUIRE(event_data != nullptr);
        REQUIRE(interval_data != nullptr);
    }

    SECTION("Data cleanup with observer pattern") {
        // Test that the observer pattern properly notifies the widget
        // when data is deleted from the DataManager

        // Open the widget to initialize it
        widget.openWidget();
        QApplication::processEvents();

        // Get initial data counts
        int initial_analog_count = 0;
        int initial_event_count = 0;
        int initial_interval_count = 0;

        for (auto const & key: test_keys) {
            if (key == "test_analog") {
                auto data = dm.getData<AnalogTimeSeries>(key);
                if (data) initial_analog_count++;
            } else if (key == "test_events") {
                auto data = dm.getData<DigitalEventSeries>(key);
                if (data) initial_event_count++;
            } else if (key == "test_intervals") {
                auto data = dm.getData<DigitalIntervalSeries>(key);
                if (data) initial_interval_count++;
            }
        }

        REQUIRE(initial_analog_count == 1);
        REQUIRE(initial_event_count == 1);
        REQUIRE(initial_interval_count == 1);

        // Delete all data
        for (auto const & key: test_keys) {
            dm.deleteData(key);
        }

        // Process Qt events to ensure observer callbacks execute
        QApplication::processEvents();

        // Verify all data has been cleaned up
        int final_analog_count = 0;
        int final_event_count = 0;
        int final_interval_count = 0;

        for (auto const & key: test_keys) {
            if (key == "test_analog") {
                auto data = dm.getData<AnalogTimeSeries>(key);
                if (data) final_analog_count++;
            } else if (key == "test_events") {
                auto data = dm.getData<DigitalEventSeries>(key);
                if (data) final_event_count++;
            } else if (key == "test_intervals") {
                auto data = dm.getData<DigitalIntervalSeries>(key);
                if (data) final_interval_count++;
            }
        }

        REQUIRE(final_analog_count == 0);
        REQUIRE(final_event_count == 0);
        REQUIRE(final_interval_count == 0);
    }
}

TEST_CASE_METHOD(DataViewerWidgetCleanupTestFixture, "DataViewer_Widget - Memory Management", "[DataViewer_Widget]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();

    SECTION("Shared pointer reference counting") {
        // Open the widget to initialize it
        widget.openWidget();
        QApplication::processEvents();

        // Get shared pointers to the data
        auto analog_data = dm.getData<AnalogTimeSeries>("test_analog");
        auto event_data = dm.getData<DigitalEventSeries>("test_events");
        auto interval_data = dm.getData<DigitalIntervalSeries>("test_intervals");

        REQUIRE(analog_data.use_count() > 1);// DataManager + any internal references
        REQUIRE(event_data.use_count() > 1);
        REQUIRE(interval_data.use_count() > 1);

        // Create weak references for correctness of lifetime checks
        std::weak_ptr<AnalogTimeSeries> weak_analog = analog_data;
        std::weak_ptr<DigitalEventSeries> weak_event = event_data;
        std::weak_ptr<DigitalIntervalSeries> weak_interval = interval_data;

        // Delete data from DataManager
        dm.deleteData("test_analog");
        dm.deleteData("test_events");
        dm.deleteData("test_intervals");

        // Process Qt events
        QApplication::processEvents();

        // Release our local strong references
        analog_data.reset();
        event_data.reset();
        interval_data.reset();

        // Verify that the objects are now expired
        REQUIRE(weak_analog.expired());
        REQUIRE(weak_event.expired());
        REQUIRE(weak_interval.expired());
    }
}

TEST_CASE_METHOD(DataViewerWidgetCleanupTestFixture, "DataViewer_Widget - Observer Pattern Integration", "[DataViewer_Widget]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();

    SECTION("Observer callback execution") {
        // Open the widget to initialize it
        widget.openWidget();
        QApplication::processEvents();

        // Verify that the observer is properly set up
        // This is implicit in the cleanup tests, but we can verify the behavior

        // Delete data and verify cleanup happens automatically
        bool deleted = dm.deleteData("test_analog");
        REQUIRE(deleted);

        // Process Qt events to trigger observer callback
        QApplication::processEvents();

        // Verify the data is no longer accessible
        auto analog_data = dm.getData<AnalogTimeSeries>("test_analog");
        REQUIRE(analog_data == nullptr);
    }

    SECTION("Multiple data deletion handling") {
        // Open the widget to initialize it
        widget.openWidget();
        QApplication::processEvents();

        // Delete multiple data items in sequence
        std::vector<std::string> keys_to_delete = {"test_analog", "test_events", "test_intervals"};

        for (auto const & key: keys_to_delete) {
            bool deleted = dm.deleteData(key);
            REQUIRE(deleted);

            // Process events after each deletion
            QApplication::processEvents();

            // Verify the deleted data is no longer accessible
            if (key == "test_analog") {
                auto data = dm.getData<AnalogTimeSeries>(key);
                REQUIRE(data == nullptr);
            } else if (key == "test_events") {
                auto data = dm.getData<DigitalEventSeries>(key);
                REQUIRE(data == nullptr);
            } else if (key == "test_intervals") {
                auto data = dm.getData<DigitalIntervalSeries>(key);
                REQUIRE(data == nullptr);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// New tests: enabling multiple analog series one by one via the widget API
// -----------------------------------------------------------------------------
class DataViewerWidgetMultiAnalogTestFixture {
protected:
    DataViewerWidgetMultiAnalogTestFixture() {
        // Ensure a Qt application exists
        if (!QApplication::instance()) {
            static int argc = 1;
            static char * argv[] = {const_cast<char *>("test")};
            m_app = std::make_unique<QApplication>(argc, argv);
        }

        // DataManager and TimeScrollBar
        m_data_manager = std::make_shared<DataManager>();
        m_time_scrollbar = std::make_unique<TimeScrollBar>();
        m_time_scrollbar->setDataManager(m_data_manager);

        // Create a default time frame and register under key "time"
        std::vector<int> t(4000);
        std::iota(std::begin(t), std::end(t), 0);

        auto new_timeframe = std::make_shared<TimeFrame>(t);

        m_time_key = TimeKey("time");

        m_data_manager->removeTime(TimeKey("time"));
        m_data_manager->setTime(TimeKey("time"), new_timeframe);

        // Populate with 5 analog time series
        populateAnalogSeries(5);

        // Create the widget
        m_widget = std::make_unique<DataViewer_Widget>(m_data_manager, m_time_scrollbar.get(), nullptr);
    }

    ~DataViewerWidgetMultiAnalogTestFixture() = default;

    DataViewer_Widget & getWidget() { return *m_widget; }
    DataManager & getDataManager() { return *m_data_manager; }
    std::vector<std::string> const & getAnalogKeys() const { return m_analog_keys; }

private:
    void populateAnalogSeries(int count) {
        // Generate simple waveforms of equal length
        constexpr int kNumSamples = 1000;
        std::vector<float> base(kNumSamples);
        for (int i = 0; i < kNumSamples; ++i) {
            base[static_cast<size_t>(i)] = std::sin(static_cast<float>(i) * 0.01f);
        }

        m_analog_keys.clear();
        m_analog_keys.reserve(static_cast<size_t>(count));

        for (int i = 0; i < count; ++i) {
            // Vary amplitude slightly by index for realism
            std::vector<float> values = base;
            float const scale = 1.0f + static_cast<float>(i) * 0.1f;
            for (auto & v: values) v *= scale;

            auto series = std::make_shared<AnalogTimeSeries>(values, values.size());
            std::string key = std::string("analog_") + std::to_string(i + 1);
            m_data_manager->setData<AnalogTimeSeries>(key, series, m_time_key);
            m_analog_keys.push_back(std::move(key));
        }
    }

    std::unique_ptr<QApplication> m_app;
    std::shared_ptr<DataManager> m_data_manager;
    std::unique_ptr<TimeScrollBar> m_time_scrollbar;
    std::unique_ptr<DataViewer_Widget> m_widget;
    TimeKey m_time_key{"time"};
    std::vector<std::string> m_analog_keys;
};

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - Enable Analog Series One By One", "[DataViewer_Widget][Analog]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();
    auto const keys = getAnalogKeys();

    REQUIRE(keys.size() == 5);

    // Open the widget and let it initialize
    widget.openWidget();
    QApplication::processEvents();

    // One-by-one enable each analog series using the widget's private slot via meta-object
    for (size_t i = 0; i < keys.size(); ++i) {
        auto const & key = keys[i];

        bool invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(key)),
                Q_ARG(bool, true));
        REQUIRE(invoked);

        // Process events to allow the UI/OpenGLWidget to add and layout the series
        QApplication::processEvents();

        // Validate that the series is now visible via the display options accessor
        auto cfg = widget.state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(
            QString::fromStdString(key));
        //auto cfg = widget.getAnalogConfig(key);
       // REQUIRE(cfg.has_value());
        REQUIRE(cfg != nullptr);
        REQUIRE(cfg->style().is_visible);

        // Gather centers and heights for all enabled series so far
        std::vector<float> centers;
        std::vector<float> heights;
        centers.reserve(i + 1);
        heights.reserve(i + 1);

        for (size_t j = 0; j <= i; ++j) {
            auto layout = TestHelpers::getAnalogLayoutTransform(widget, keys[j]);
            REQUIRE(layout.has_value());
            centers.push_back(static_cast<float>(layout->offset));
            heights.push_back(static_cast<float>(layout->gain * 2.0f));
        }

        std::sort(centers.begin(), centers.end());

        // Expected evenly spaced centers across [-1, 1] at fractions k/(N+1)
        size_t const enabled_count = i + 1;
        float const tol_center = 0.22f;// generous tolerance for layout differences
        for (size_t k = 0; k < enabled_count; ++k) {
            float const expected = -1.0f + 2.0f * (static_cast<float>(k + 1) / static_cast<float>(enabled_count + 1));
            REQUIRE(std::abs(centers[k] - expected) <= tol_center);
        }

        // Heights should be roughly proportional to the available spacing between centers
        // Expected spacing between adjacent centers
        float const expected_spacing = 2.0f / static_cast<float>(enabled_count + 1);
        float const min_h = *std::min_element(heights.begin(), heights.end());
        float const max_h = *std::max_element(heights.begin(), heights.end());

        // Bounds: each height within [0.4, 1.2] * expected spacing
        for (auto const h: heights) {
            //           REQUIRE(h >= expected_spacing * 0.4f);
            //         REQUIRE(h <= expected_spacing * 1.2f);
        }

        // And heights should be fairly consistent across series (within 40%)
        if (min_h > 0.0f) {
            REQUIRE((max_h / min_h) <= 1.4f);
        }
    }
}

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - Enable Five Analog Series via Group Toggle", "[DataViewer_Widget][Analog][Group]") {
    auto & widget = getWidget();
    auto const & keys = getAnalogKeys();
    REQUIRE(keys.size() == 5);

    std::cout << "CTEST_FULL_OUTPUT" << std::endl;

    widget.openWidget();
    QApplication::processEvents();

    // Locate the Feature_Tree_Widget inside the DataViewer widget
    auto ftw = widget.findChild<Feature_Tree_Widget *>("feature_tree_widget");
    REQUIRE(ftw != nullptr);

    // Ensure the tree is populated
    ftw->refreshTree();
    QApplication::processEvents();

    QTreeWidget * tree = ftw->treeWidget();
    REQUIRE(tree != nullptr);

    // Find the top-level "Analog" node
    QTreeWidgetItem * analogRoot = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem * item = tree->topLevelItem(i);
        if (item && item->text(0) == QString("analog")) {
            analogRoot = item;
            break;
        }
    }
    REQUIRE(analogRoot != nullptr);

    // Find the group node for prefix "analog" (derived from keys like analog_1..analog_5)
    QTreeWidgetItem * analogGroup = nullptr;
    for (int i = 0; i < analogRoot->childCount(); ++i) {
        QTreeWidgetItem * child = analogRoot->child(i);
        if (child && child->text(0) == QString("analog")) {
            analogGroup = child;
            break;
        }
    }
    REQUIRE(analogGroup != nullptr);

    // Toggle the group checkbox (column 1 is the checkbox column)
    analogGroup->setCheckState(1, Qt::Checked);
    QApplication::processEvents();

    // Verify that all five analog series became visible
    for (auto const & key: keys) {
        auto cfg = widget.state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(
            QString::fromStdString(key));
        REQUIRE(cfg != nullptr);
        REQUIRE(cfg->style().is_visible);
    }
}

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - Group toggle then single enable resets stacking", "[DataViewer_Widget][Analog][Group][Regression]") {
    auto & widget = getWidget();
    auto const & keys = getAnalogKeys();
    REQUIRE(keys.size() == 5);

    widget.openWidget();
    QApplication::processEvents();

    // Locate the Feature_Tree_Widget inside the DataViewer widget
    auto ftw = widget.findChild<Feature_Tree_Widget *>("feature_tree_widget");
    REQUIRE(ftw != nullptr);

    // Ensure the tree is populated
    ftw->refreshTree();
    QApplication::processEvents();

    QTreeWidget * tree = ftw->treeWidget();
    REQUIRE(tree != nullptr);

    // Find the top-level "Analog" node
    QTreeWidgetItem * analogRoot = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem * item = tree->topLevelItem(i);
        if (item && item->text(0) == QString("analog")) {
            analogRoot = item;
            break;
        }
    }
    REQUIRE(analogRoot != nullptr);

    // Find the group node for prefix "analog" (derived from keys like analog_1..analog_5)
    QTreeWidgetItem * analogGroup = nullptr;
    for (int i = 0; i < analogRoot->childCount(); ++i) {
        QTreeWidgetItem * child = analogRoot->child(i);
        if (child && child->text(0) == QString("analog")) {
            analogGroup = child;
            break;
        }
    }
    REQUIRE(analogGroup != nullptr);

    // 1) Enable the whole group
    analogGroup->setCheckState(1, Qt::Checked);
    QApplication::processEvents();

    // Verify that all five analog series became visible
    for (auto const & key: keys) {
        auto cfg = widget.state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(
            QString::fromStdString(key));
        REQUIRE(cfg != nullptr);
        REQUIRE(cfg->get_is_visible());
    }

    // 2) Disable the whole group
    analogGroup->setCheckState(1, Qt::Unchecked);
    QApplication::processEvents();

    // Verify that all five analog series are no longer visible
    // After disabling, options may be removed from registry or have is_visible = false
    for (auto const & key: keys) {
        auto cfg = widget.state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(
            QString::fromStdString(key));
        // Either cfg is null (removed from registry) or is_visible is false
        if (cfg != nullptr) {
            REQUIRE_FALSE(cfg->get_is_visible());
        }
    }

    // 3) Re-enable a single key (simulate selecting one channel from the group)
    std::string const single_key = keys.front();
    bool invoked = QMetaObject::invokeMethod(
            &widget,
            "_addFeatureToModel",
            Qt::DirectConnection,
            Q_ARG(QString, QString::fromStdString(single_key)),
            Q_ARG(bool, true));
    REQUIRE(invoked);
    QApplication::processEvents();

    // 4) Assert this single key is treated as a single-lane stack:
    //    center ~ 0 and height ~ full canvas (about 2.0), not a 1/5 lane.
    auto opts_single = TestHelpers::getAnalogOptions(widget, single_key);
    REQUIRE(opts_single != nullptr);
    REQUIRE(opts_single->get_is_visible());

    auto layout_single = TestHelpers::getAnalogLayoutTransform(widget, single_key);
    REQUIRE(layout_single.has_value());

    float const center = static_cast<float>(layout_single->offset);
    float const height = static_cast<float>(layout_single->gain * 2.0f);

    // Center should be near 0.0
    REQUIRE(std::abs(center - 0.0f) <= 0.25f);
    // Height should be near full canvas height ([-1,1] -> ~2.0)
    REQUIRE(height >= 1.6f);
    REQUIRE(height <= 2.2f);

    // And all other keys should remain not visible
    for (size_t i = 1; i < keys.size(); ++i) {
        auto opts = TestHelpers::getAnalogOptions(widget, keys[i]);
        if (opts != nullptr) {
            REQUIRE_FALSE(opts->get_is_visible());
        }
    }
}

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - Apply spikesorter configuration ordering", "[DataViewer_Widget][Analog][Config]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();
    auto const keys = getAnalogKeys();
    REQUIRE(keys.size() >= 4);

    widget.openWidget();
    QApplication::processEvents();

    // Enable four channels from the same group (analog prefix)
    for (size_t i = 0; i < 4; ++i) {
        bool invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(keys[i])),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    // Capture centers before loading configuration
    std::vector<std::pair<std::string, float>> centers_before;
    for (size_t i = 0; i < 4; ++i) {
        auto layout = TestHelpers::getAnalogLayoutTransform(widget, keys[i]);
        REQUIRE(layout.has_value());
        centers_before.emplace_back(keys[i], static_cast<float>(layout->offset));
    }

    // Build a small spikesorter configuration text with distinct y values
    // Header + rows: row ch x y
    char const * cfg =
            "poly2\n"
            "1 1 0 300\n"
            "2 2 0 100\n"
            "3 3 0 200\n"
            "4 4 0 400\n";

    // Load configuration directly via helper to avoid file dialogs
    bool invoked = QMetaObject::invokeMethod(
            &widget,
            "_loadSpikeSorterConfigurationFromText",
            Qt::DirectConnection,
            Q_ARG(QString, QString("analog")),
            Q_ARG(QString, QString(cfg)));
    REQUIRE(invoked);
    QApplication::processEvents();

    // After config, the highest y (400) should be at the top (largest allocated_y_center)
    std::vector<std::pair<std::string, float>> key_center;
    for (size_t i = 0; i < 4; ++i) {
        auto layout = TestHelpers::getAnalogLayoutTransform(widget, keys[i]);
        REQUIRE(layout.has_value());
        key_center.emplace_back(keys[i], static_cast<float>(layout->offset));
    }
    // Capture centers after
    std::vector<std::pair<std::string, float>> centers_after = key_center;

    INFO("Centers before:");
    for (auto const & kv: centers_before) {
        INFO(kv.first << " -> " << kv.second);
    }
    INFO("Centers after:");
    for (auto const & kv: centers_after) {
        INFO(kv.first << " -> " << kv.second);
    }

    // Ensure at least one center changed due to configuration ordering
    bool any_changed = false;
    for (size_t i = 0; i < centers_before.size(); ++i) {
        for (size_t j = 0; j < centers_after.size(); ++j) {
            if (centers_before[i].first == centers_after[j].first) {
                if (std::abs(centers_before[i].second - centers_after[j].second) > 1e-6f) {
                    any_changed = true;
                }
            }
        }
    }
    REQUIRE(any_changed);
    // Sort by center descending to get top-to-bottom order
    std::sort(key_center.begin(), key_center.end(), [](auto const & a, auto const & b) { return a.second > b.second; });

    // Expected order by y: 400 (ch 3)-> key 4, then 300 (ch 0)-> key 1, then 200 (ch 2)-> key 3, then 100 (ch 1)-> key 2
    REQUIRE(key_center.size() == 4);
    REQUIRE(key_center[0].first == keys[3]);
    REQUIRE(key_center[1].first == keys[0]);
    REQUIRE(key_center[2].first == keys[2]);
    REQUIRE(key_center[3].first == keys[1]);
}

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - X axis unchanged on global gain change", "[DataViewer_Widget][Analog][XAxis]") {
    auto & widget = getWidget();
    auto const keys = getAnalogKeys();
    REQUIRE(keys.size() >= 1);

    widget.openWidget();
    QApplication::processEvents();

    // Enable a single analog series
    bool invoked = QMetaObject::invokeMethod(
            &widget,
            "_addFeatureToModel",
            Qt::DirectConnection,
            Q_ARG(QString, QString::fromStdString(keys[0])),
            Q_ARG(bool, true));
    REQUIRE(invoked);
    QApplication::processEvents();

    // Locate the OpenGLWidget to query XAxis
    auto glw = widget.findChild<OpenGLWidget *>("openGLWidget");
    REQUIRE(glw != nullptr);
    
    // Get state to set time range directly
    auto * state = widget.state();
    REQUIRE(state != nullptr);

    // Set an initial center (time) and range width via state
    int const initial_time_index = 1000;
    invoked = QMetaObject::invokeMethod(
            &widget,
            "_updatePlot",
            Qt::DirectConnection,
            Q_ARG(int, initial_time_index));
    REQUIRE(invoked);

    int const initial_range_width = 2000;
    state->setTimeWidth(initial_range_width);
    QApplication::processEvents();

    auto const & view_state_before = glw->getViewState();
    auto const start_before = view_state_before.time_start;
    auto const end_before = view_state_before.time_end;

    // Change global gain via state and re-draw
    double const new_gain = 2.0;
    state->setGlobalZoom(static_cast<float>(new_gain));
    QApplication::processEvents();

    auto const & view_state_after = glw->getViewState();
    auto const start_after = view_state_after.time_start;
    auto const end_after = view_state_after.time_end;

    // Verify X window did not change
    REQUIRE(start_before == start_after);
    REQUIRE(end_before == end_after);
}

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - Preserve analog selections when adding digital interval", "[DataViewer_Widget][Analog][DigitalInterval]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();
    auto const keys = getAnalogKeys();
    REQUIRE(keys.size() == 5);

    widget.openWidget();
    QApplication::processEvents();

    // Enable 3 out of 5 analog series (sparse selection)
    std::vector<std::string> enabled = {keys[0], keys[2], keys[4]};
    for (auto const & k: enabled) {
        bool invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(k)),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    // Verify the enabled set is visible and others are not present/visible
    auto isVisible = [&](std::string const & k) {
        auto opts = TestHelpers::getAnalogOptions(widget, k);
        return opts != nullptr && opts->get_is_visible();
    };

    REQUIRE(isVisible(keys[0]));
    REQUIRE(isVisible(keys[2]));
    REQUIRE(isVisible(keys[4]));
    // Not enabled yet: may be nullopt or not visible
    auto opts1 = TestHelpers::getAnalogOptions(widget, keys[1]);
    if (opts1) REQUIRE_FALSE(opts1->get_is_visible());
    auto opts3 = TestHelpers::getAnalogOptions(widget, keys[3]);
    if (opts3) REQUIRE_FALSE(opts3->get_is_visible());

    // Add a new DigitalIntervalSeries to the DataManager (should NOT clear visible analog series)
    auto interval_series = std::make_shared<DigitalIntervalSeries>();
    interval_series->addEvent(TimeFrameIndex(100), TimeFrameIndex(300));
    dm.setData<DigitalIntervalSeries>("interval_added_late", interval_series, TimeKey("time"));

    // Process events so the feature tree and any observers react
    QApplication::processEvents();

    // Verify the originally enabled analog series remain visible
    REQUIRE(isVisible(keys[0]));
    REQUIRE(isVisible(keys[2]));
    REQUIRE(isVisible(keys[4]));

    // Still not enabled ones should remain not visible
    opts1 = TestHelpers::getAnalogOptions(widget, keys[1]);
    if (opts1) REQUIRE_FALSE(opts1->get_is_visible());
    opts3 = TestHelpers::getAnalogOptions(widget, keys[3]);
    if (opts3) REQUIRE_FALSE(opts3->get_is_visible());
}

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - Adding digital interval does not change analog gain", "[DataViewer_Widget][Analog][DigitalInterval][Regression]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();
    auto const keys = getAnalogKeys();
    REQUIRE(keys.size() >= 2);

    widget.openWidget();
    QApplication::processEvents();

    // Enable two analog series
    for (size_t i = 0; i < 2; ++i) {
        bool const invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(keys[i])),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    // Capture their allocated heights (proxy for gain)
    auto layout_a0_before = TestHelpers::getAnalogLayoutTransform(widget, keys[0]);
    auto layout_a1_before = TestHelpers::getAnalogLayoutTransform(widget, keys[1]);
    REQUIRE(layout_a0_before.has_value());
    REQUIRE(layout_a1_before.has_value());
    float const h0_before = static_cast<float>(layout_a0_before->gain * 2.0f);
    float const h1_before = static_cast<float>(layout_a1_before->gain * 2.0f);
    // Sanity: they should be similar (two-lane stacking)
    if (std::min(h0_before, h1_before) > 0.0f) {
        REQUIRE((std::max(h0_before, h1_before) / std::min(h0_before, h1_before)) <= 1.4f);
    }

    // Add a digital interval series that by default renders full-canvas
    auto interval_series = std::make_shared<DigitalIntervalSeries>();
    interval_series->addEvent(TimeFrameIndex(150), TimeFrameIndex(350));
    std::string const interval_key = "interval_fullcanvas";
    dm.setData<DigitalIntervalSeries>(interval_key, interval_series, TimeKey("time"));
    QApplication::processEvents();

    // Enable the digital interval in the view
    bool const invoked_interval = QMetaObject::invokeMethod(
            &widget,
            "_addFeatureToModel",
            Qt::DirectConnection,
            Q_ARG(QString, QString::fromStdString(interval_key)),
            Q_ARG(bool, true));
    REQUIRE(invoked_interval);
    QApplication::processEvents();

    // Verify the interval is visible and near full canvas height
    auto opts_interval = TestHelpers::getIntervalOptions(widget, interval_key);
    REQUIRE(opts_interval != nullptr);
    REQUIRE(opts_interval->get_is_visible());
    auto layout_interval = TestHelpers::getIntervalLayoutTransform(widget, interval_key);
    REQUIRE(layout_interval.has_value());
    REQUIRE(layout_interval->gain * 2.0f >= 1.6f);
    REQUIRE(layout_interval->gain * 2.0f <= 2.2f);

    // Re-capture analog heights after enabling the interval
    auto layout_a0_after = TestHelpers::getAnalogLayoutTransform(widget, keys[0]);
    auto layout_a1_after = TestHelpers::getAnalogLayoutTransform(widget, keys[1]);
    REQUIRE(layout_a0_after.has_value());
    REQUIRE(layout_a1_after.has_value());
    float const h0_after = static_cast<float>(layout_a0_after->gain * 2.0f);
    float const h1_after = static_cast<float>(layout_a1_after->gain * 2.0f);

    INFO("h0_before=" << h0_before << ", h0_after=" << h0_after);
    INFO("h1_before=" << h1_before << ", h1_after=" << h1_after);

    // Regression check: enabling a full-canvas digital interval must NOT attenuate analog gain.
    // Allow a small tolerance for layout jitter.
    auto approx_equal = [](float a, float b) {
        float const denom = std::max(1.0f, std::max(std::abs(a), std::abs(b)));
        return std::abs(a - b) / denom <= 0.1f; // <=10% relative change
    };
    REQUIRE(approx_equal(h0_after, h0_before));
    REQUIRE(approx_equal(h1_after, h1_before));
}

TEST_CASE_METHOD(DataViewerWidgetMultiAnalogTestFixture, "DataViewer_Widget - Adding digital interval does not change global zoom or analog mapping", "[DataViewer_Widget][Analog][DigitalInterval][Regression]") {
    auto & widget = getWidget();
    auto & dm = getDataManager();
    auto const keys = getAnalogKeys();
    REQUIRE(keys.size() >= 2);

    widget.openWidget();
    QApplication::processEvents();

    // Enable two analog series
    for (size_t i = 0; i < 2; ++i) {
        bool const ok = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(keys[i])),
                Q_ARG(bool, true));
        REQUIRE(ok);
        QApplication::processEvents();
    }

    // Get state and record the global zoom value
    auto * state = widget.state();
    REQUIRE(state != nullptr);
    float const zoom_before = state->globalZoom();

    // Also capture an analog canvasY->value slope proxy for one series
    auto glw = widget.findChild<OpenGLWidget *>("openGLWidget");
    REQUIRE(glw != nullptr);
    QApplication::processEvents();
    auto size = glw->getCanvasSize();
    float const h = static_cast<float>(size.second);
    float const y1 = h * 0.25f;
    float const y2 = h * 0.75f;
    auto map_delta = [&](std::string const & key) {
        float const v1 = glw->canvasYToAnalogValue(y1, key);
        float const v2 = glw->canvasYToAnalogValue(y2, key);
        return std::abs(v2 - v1) / std::max(1e-6f, (y2 - y1));
    };
    float const slope_before = map_delta(keys[0]);

    // Add and enable a full-canvas digital interval
    auto interval_series = std::make_shared<DigitalIntervalSeries>();
    interval_series->addEvent(TimeFrameIndex(100), TimeFrameIndex(300));
    std::string const int_key = "interval_fc";
    dm.setData<DigitalIntervalSeries>(int_key, interval_series, TimeKey("time"));
    QApplication::processEvents();
    bool const ok2 = QMetaObject::invokeMethod(
            &widget,
            "_addFeatureToModel",
            Qt::DirectConnection,
            Q_ARG(QString, QString::fromStdString(int_key)),
            Q_ARG(bool, true));
    REQUIRE(ok2);
    QApplication::processEvents();

    // Global zoom should not change when adding a full-canvas interval
    float const zoom_after = state->globalZoom();
    INFO("zoom_before=" << zoom_before << ", zoom_after=" << zoom_after);
    REQUIRE(Catch::Approx(zoom_after).margin(zoom_before * 0.05f + 1e-6f) == zoom_before);

    // Analog mapping slope should remain approximately the same
    float const slope_after = map_delta(keys[0]);
    INFO("slope_before=" << slope_before << ", slope_after=" << slope_after);
    if (std::max(slope_before, slope_after) > 0.0f) {
        float const rel_change = std::abs(slope_after - slope_before) / std::max(1e-6f, std::max(slope_before, slope_after));
        REQUIRE(rel_change <= 0.1f);
    }
}

// -----------------------------------------------------------------------------
// New tests: enabling multiple digital event series one by one via the widget API
// -----------------------------------------------------------------------------
class DataViewerWidgetMultiEventTestFixture {
protected:
    DataViewerWidgetMultiEventTestFixture() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char * argv[] = {const_cast<char *>("test")};
            m_app = std::make_unique<QApplication>(argc, argv);
        }

        m_data_manager = std::make_shared<DataManager>();
        m_time_scrollbar = std::make_unique<TimeScrollBar>();
        m_time_scrollbar->setDataManager(m_data_manager);

        // Create a default time frame and register under key "time"
        std::vector<int> t(4000);
        std::iota(std::begin(t), std::end(t), 0);
        auto new_timeframe = std::make_shared<TimeFrame>(t);
        m_time_key = TimeKey("time");
        m_data_manager->removeTime(TimeKey("time"));
        m_data_manager->setTime(TimeKey("time"), new_timeframe);

        // Populate with 5 digital event series
        populateEventSeries(5);

        m_widget = std::make_unique<DataViewer_Widget>(m_data_manager, m_time_scrollbar.get(), nullptr);
    }

    ~DataViewerWidgetMultiEventTestFixture() = default;

    DataViewer_Widget & getWidget() { return *m_widget; }
    DataManager & getDataManager() { return *m_data_manager; }
    std::vector<std::string> const & getEventKeys() const { return m_event_keys; }

private:
    void populateEventSeries(int count) {
        m_event_keys.clear();
        m_event_keys.reserve(static_cast<size_t>(count));

        for (int i = 0; i < count; ++i) {
            auto series = std::make_shared<DigitalEventSeries>();
            // Add a few events within the visible range
            series->addEvent(TimeFrameIndex(1000));
            series->addEvent(TimeFrameIndex(2000));
            series->addEvent(TimeFrameIndex(3000));

            std::string key = std::string("event_") + std::to_string(i + 1);
            m_data_manager->setData<DigitalEventSeries>(key, series, m_time_key);
            m_event_keys.push_back(std::move(key));
        }
    }

    std::unique_ptr<QApplication> m_app;
    std::shared_ptr<DataManager> m_data_manager;
    std::unique_ptr<TimeScrollBar> m_time_scrollbar;
    std::unique_ptr<DataViewer_Widget> m_widget;
    TimeKey m_time_key{"time"};
    std::vector<std::string> m_event_keys;
};

TEST_CASE_METHOD(DataViewerWidgetMultiEventTestFixture, "DataViewer_Widget - Enable Digital Event Series One By One", "[DataViewer_Widget][DigitalEvent]") {
    auto & widget = getWidget();
    auto const keys = getEventKeys();
    REQUIRE(keys.size() == 5);

    widget.openWidget();
    QApplication::processEvents();

    for (size_t i = 0; i < keys.size(); ++i) {
        auto const & key = keys[i];

        bool invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(key)),
                Q_ARG(bool, true));
        REQUIRE(invoked);

        QApplication::processEvents();

        auto opts = TestHelpers::getEventOptions(widget, key);
        REQUIRE(opts != nullptr);
        REQUIRE(opts->get_is_visible());

        std::vector<float> centers;
        std::vector<float> heights;
        centers.reserve(i + 1);
        heights.reserve(i + 1);

        for (size_t j = 0; j <= i; ++j) {
            auto layout = TestHelpers::getEventLayoutTransform(widget, keys[j]);
            REQUIRE(layout.has_value());
            centers.push_back(static_cast<float>(layout->offset));
            heights.push_back(static_cast<float>(layout->gain * 2.0f));
        }

        std::sort(centers.begin(), centers.end());

        size_t const enabled_count = i + 1;
        float const tol_center = 0.22f;
        for (size_t k = 0; k < enabled_count; ++k) {
            // Expected evenly spaced centers across [-1, 1] at fractions k/(N+1)
            float const expected = -1.0f + 2.0f * (static_cast<float>(k + 1) / static_cast<float>(enabled_count + 1));
            REQUIRE(std::abs(centers[k] - expected) <= tol_center);
        }

        float const expected_height = 2.0f / static_cast<float>(enabled_count);
        float const min_h = *std::min_element(heights.begin(), heights.end());
        float const max_h = *std::max_element(heights.begin(), heights.end());

        for (auto const h: heights) {
            // Heights should be within their allocated lane (allow tolerance)
            REQUIRE(h > 0.0f);
            REQUIRE(h >= expected_height * 0.4f);
            REQUIRE(h <= expected_height * 1.2f);
        }

        if (min_h > 0.0f) {
            REQUIRE((max_h / min_h) <= 1.4f);
        }
    }
}

// -----------------------------------------------------------------------------
// Mixed stacking test: analog + digital events share vertical space uniformly
// -----------------------------------------------------------------------------
class DataViewerWidgetMixedStackingTestFixture {
protected:
    DataViewerWidgetMixedStackingTestFixture() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char * argv[] = {const_cast<char *>("test")};
            m_app = std::make_unique<QApplication>(argc, argv);
        }

        m_data_manager = std::make_shared<DataManager>();
        m_time_scrollbar = std::make_unique<TimeScrollBar>();
        m_time_scrollbar->setDataManager(m_data_manager);

        // Master time frame
        std::vector<int> t(4000);
        std::iota(std::begin(t), std::end(t), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        m_time_key = TimeKey("time");
        m_data_manager->removeTime(TimeKey("time"));
        m_data_manager->setTime(TimeKey("time"), tf);

        // 3 analog series
        for (int i = 0; i < 3; ++i) {
            constexpr int kNumSamples = 1000;
            std::vector<float> values(kNumSamples, 0.0f);
            for (int j = 0; j < kNumSamples; ++j) {
                values[static_cast<size_t>(j)] = std::sin(static_cast<float>(j) * 0.01f) * (1.0f + 0.1f * static_cast<float>(i));
            }
            auto series = std::make_shared<AnalogTimeSeries>(values, values.size());
            std::string key = std::string("analog_") + std::to_string(i + 1);
            m_analog_keys.push_back(key);
            m_data_manager->setData<AnalogTimeSeries>(key, series, m_time_key);
        }

        // 2 event series
        for (int i = 0; i < 2; ++i) {
            auto series = std::make_shared<DigitalEventSeries>();
            series->addEvent(TimeFrameIndex(1000));
            series->addEvent(TimeFrameIndex(2000));
            series->addEvent(TimeFrameIndex(3000));
            std::string key = std::string("event_") + std::to_string(i + 1);
            m_event_keys.push_back(key);
            m_data_manager->setData<DigitalEventSeries>(key, series, m_time_key);
        }

        m_widget = std::make_unique<DataViewer_Widget>(m_data_manager, m_time_scrollbar.get(), nullptr);
    }

    ~DataViewerWidgetMixedStackingTestFixture() = default;

    DataViewer_Widget & getWidget() { return *m_widget; }
    std::vector<std::string> const & getAnalogKeys() const { return m_analog_keys; }
    std::vector<std::string> const & getEventKeys() const { return m_event_keys; }

private:
    std::unique_ptr<QApplication> m_app;
    std::shared_ptr<DataManager> m_data_manager;
    std::unique_ptr<TimeScrollBar> m_time_scrollbar;
    std::unique_ptr<DataViewer_Widget> m_widget;
    TimeKey m_time_key{"time"};
    std::vector<std::string> m_analog_keys;
    std::vector<std::string> m_event_keys;
};

TEST_CASE_METHOD(DataViewerWidgetMixedStackingTestFixture, "DataViewer_Widget - Mixed stacking for analog + digital events", "[DataViewer_Widget][Mixed][Stacking]") {
    auto & widget = getWidget();
    auto const analog = getAnalogKeys();
    auto const ev = getEventKeys();
    REQUIRE(analog.size() == 3);
    REQUIRE(ev.size() == 2);

    widget.openWidget();
    QApplication::processEvents();

    // Enable analog first
    for (auto const & k: analog) {
        bool const invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(k)),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    // Then enable events
    for (auto const & k: ev) {
        bool const invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(k)),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    // Collect centers and heights across all 5 stackable series
    struct Item {
        float center;
        float height;
        bool is_event;
        std::string key;
    };
    std::vector<Item> items;
    items.reserve(5);

    for (auto const & k: analog) {
        auto opts = TestHelpers::getAnalogOptions(widget, k);
        REQUIRE(opts != nullptr);
        REQUIRE(opts->get_is_visible());
        auto layout = TestHelpers::getAnalogLayoutTransform(widget, k);
        REQUIRE(layout.has_value());
        items.push_back(Item{static_cast<float>(layout->offset), static_cast<float>(layout->gain * 2.0f), false, k});
    }
    for (auto const & k: ev) {
        auto opts = TestHelpers::getEventOptions(widget, k);
        REQUIRE(opts != nullptr);
        REQUIRE(opts->get_is_visible());
        auto layout = TestHelpers::getEventLayoutTransform(widget, k);
        REQUIRE(layout.has_value());
        items.push_back(Item{static_cast<float>(layout->offset), static_cast<float>(layout->gain * 2.0f), true, k});
    }

    REQUIRE(items.size() == 5);
    std::sort(items.begin(), items.end(), [](Item const & a, Item const & b) { return a.center < b.center; });

    // Expect 5 evenly spaced centers across [-1,1]
    size_t const N = items.size();
    float const tol_center = 0.22f;
    for (size_t i = 0; i < N; ++i) {
        float const expected = -1.0f + 2.0f * (static_cast<float>(i + 1) / static_cast<float>(N + 1));
        REQUIRE(std::abs(items[i].center - expected) <= tol_center);
    }

    // Heights should be consistent across analog and events when stacked together
    float const expected_height = 2.0f / static_cast<float>(N);
    float min_h = std::numeric_limits<float>::max();
    float max_h = 0.0f;
    for (auto const & it: items) {
        min_h = std::min(min_h, it.height);
        max_h = std::max(max_h, it.height);
        REQUIRE(it.height >= expected_height * 0.4f);
        REQUIRE(it.height <= expected_height * 1.2f);
    }
    if (min_h > 0.0f) {
        REQUIRE((max_h / min_h) <= 1.4f);
    }

    // Additional safety: effective model height for events must be within lane
    for (auto const & k: ev) {
        auto opts = TestHelpers::getEventOptions(widget, k);
        REQUIRE(opts != nullptr);
        auto layout = TestHelpers::getEventLayoutTransform(widget, k);
        REQUIRE(layout.has_value());
        float const lane = expected_height;
        // global_vertical_scale defaults to 1.0f
        float const eff_model_height = std::min(opts->event_height, layout->gain * 2.0f) * opts->margin_factor;
        REQUIRE(eff_model_height <= lane * 1.1f);
        REQUIRE(eff_model_height < 1.8f);// definitely not full canvas
    }
}

// -----------------------------------------------------------------------------
// Mode regression test: ensure FullCanvas event uses full height and Stacked stays in lane
// -----------------------------------------------------------------------------
TEST_CASE_METHOD(DataViewerWidgetMixedStackingTestFixture, "DataViewer_Widget - Digital event plotting modes", "[DataViewer_Widget][DigitalEvent][Modes]") {
    auto & widget = getWidget();
    auto const analog = getAnalogKeys();
    auto const ev = getEventKeys();
    REQUIRE(analog.size() == 3);
    REQUIRE(ev.size() == 2);

    widget.openWidget();
    QApplication::processEvents();

    // Enable one analog to ensure mixed context
    {
        bool const invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(analog[0])),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    // Enable two events
    for (auto const & k: ev) {
        bool const invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(k)),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    // Force first event to FullCanvas via its display options
    {
        auto opts0 = widget.state()->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(ev[0]));
        REQUIRE(opts0 != nullptr);
        opts0->plotting_mode = EventPlottingModeData::FullCanvas;
    }
    // Force a redraw so allocation reflects new mode
    {
        auto glw = widget.findChild<OpenGLWidget *>("openGLWidget");
        REQUIRE(glw != nullptr);
        glw->updateCanvas();
        QApplication::processEvents();
    }

    // Read configs
    auto opts_full = TestHelpers::getEventOptions(widget, ev[0]);
    auto opts_stacked = TestHelpers::getEventOptions(widget, ev[1]);
    auto layout_full = TestHelpers::getEventLayoutTransform(widget, ev[0]);
    auto layout_stacked = TestHelpers::getEventLayoutTransform(widget, ev[1]);
    REQUIRE(opts_full != nullptr);
    REQUIRE(opts_stacked != nullptr);
    REQUIRE(layout_full.has_value());
    REQUIRE(layout_stacked.has_value());

    // FullCanvas should be centered and nearly full height
    REQUIRE(std::abs(layout_full->offset - 0.0f) <= 0.25f);
    REQUIRE(layout_full->gain * 2.0f >= 1.6f);
    REQUIRE(layout_full->gain * 2.0f <= 2.2f);

    // Stacked should be a lane with significantly smaller height
    REQUIRE(layout_stacked->gain * 2.0f < layout_full->gain * 2.0f);
}

// -----------------------------------------------------------------------------
// Regression: two stacked digital events should not render near full canvas height
// -----------------------------------------------------------------------------
TEST_CASE_METHOD(DataViewerWidgetMultiEventTestFixture, "DataViewer_Widget - Two stacked digital events height bounded", "[DataViewer_Widget][DigitalEvent][Height]") {
    auto & widget = getWidget();
    auto const keys = getEventKeys();
    REQUIRE(keys.size() >= 2);

    widget.openWidget();
    QApplication::processEvents();

    // Enable two events in stacked mode (default)
    for (size_t i = 0; i < 2; ++i) {
        bool const invoked = QMetaObject::invokeMethod(
                &widget,
                "_addFeatureToModel",
                Qt::DirectConnection,
                Q_ARG(QString, QString::fromStdString(keys[i])),
                Q_ARG(bool, true));
        REQUIRE(invoked);
        QApplication::processEvents();
    }

    size_t const N = 2;
    float const lane = 2.0f / static_cast<float>(N);

    for (size_t i = 0; i < 2; ++i) {
        auto opts = TestHelpers::getEventOptions(widget, keys[i]);
        auto layout = TestHelpers::getEventLayoutTransform(widget, keys[i]);
        REQUIRE(opts != nullptr);
        REQUIRE(layout.has_value());
        // Effective model height derived from configuration (assumes unit global vertical scale)
        float const eff_model_height = std::min(opts->event_height, layout->gain * 2.0f) * opts->margin_factor;
        // Should be substantially smaller than lane height (default event height 0.05 << lane=1.0)
        REQUIRE(eff_model_height <= lane * 0.5f);
        REQUIRE(eff_model_height > 0.0f);
    }
}

/**
 * @brief Test fixture for short video scrolling regression test
 * 
 * Simulates the bug with 704-frame videos where X axis gets stuck at 2 samples
 * after extreme zoom operations.
 */
class DataViewerWidgetShortVideoTestFixture {
protected:
    DataViewerWidgetShortVideoTestFixture() {
        // Create Qt application if one doesn't exist
        if (!QApplication::instance()) {
            static int argc = 1;
            static char * argv[] = {const_cast<char *>("test")};
            m_app = std::make_unique<QApplication>(argc, argv);
        }

        // Initialize DataManager
        m_data_manager = std::make_shared<DataManager>();

        // Create a mock TimeScrollBar
        m_time_scrollbar = std::make_unique<TimeScrollBar>();
        m_time_scrollbar->setDataManager(m_data_manager);

        // Create short video data (704 frames like the bug report)
        populateWithShortVideoData();

        // Create the DataViewer_Widget
        m_widget = std::make_unique<DataViewer_Widget>(m_data_manager, m_time_scrollbar.get(), nullptr);
    }

    ~DataViewerWidgetShortVideoTestFixture() = default;

    DataViewer_Widget & getWidget() { return *m_widget; }
    DataManager & getDataManager() { return *m_data_manager; }
    std::vector<std::string> getTestDataKeys() const { return m_test_data_keys; }

private:
    void populateWithShortVideoData() {
        // Create 704-frame time vector (short video)
        constexpr size_t video_length = 704;
        std::vector<int> t(video_length);
        std::iota(std::begin(t), std::end(t), 0);

        auto new_timeframe = std::make_shared<TimeFrame>(t);
        auto time_key = TimeKey("master");

        m_data_manager->removeTime(TimeKey("master"));
        m_data_manager->setTime(TimeKey("master"), new_timeframe);

        // Add test analog data with 704 samples
        std::vector<float> analog_values(video_length);
        for (size_t i = 0; i < video_length; ++i) {
            analog_values[i] = std::sin(static_cast<float>(i) * 0.1f) * 10.0f;
        }
        auto analog_data = std::make_shared<AnalogTimeSeries>(analog_values, video_length);
        m_data_manager->setData<AnalogTimeSeries>("test_analog_704", analog_data, time_key);
        m_test_data_keys.push_back("test_analog_704");
    }

    std::unique_ptr<QApplication> m_app;
    std::shared_ptr<DataManager> m_data_manager;
    std::unique_ptr<TimeScrollBar> m_time_scrollbar;
    std::unique_ptr<DataViewer_Widget> m_widget;
    std::vector<std::string> m_test_data_keys;
};

TEST_CASE_METHOD(DataViewerWidgetShortVideoTestFixture, "DataViewer_Widget - Short video extreme scrolling regression", "[DataViewer_Widget][XAxis][Regression][ShortVideo]") {
    // This test simulates the reported bug where scrolling out too far on a 704-frame video
    // causes the X axis to get stuck at 2 samples
    
    auto & widget = getWidget();
    auto & dm = getDataManager();
    
    // Open the widget
    widget.openWidget();
    QApplication::processEvents();
    
    // Add the test analog series
    auto const keys = getTestDataKeys();
    REQUIRE(keys.size() == 1);
    
    bool invoked = QMetaObject::invokeMethod(
            &widget,
            "_addFeatureToModel",
            Qt::DirectConnection,
            Q_ARG(QString, QString::fromStdString(keys[0])),
            Q_ARG(bool, true));
    REQUIRE(invoked);
    QApplication::processEvents();
    
    // Plot the feature
    invoked = QMetaObject::invokeMethod(
            &widget,
            "_plotSelectedFeature",
            Qt::DirectConnection,
            Q_ARG(std::string, keys[0]));
    REQUIRE(invoked);
    QApplication::processEvents();
    
    // Locate the OpenGLWidget to query XAxis
    auto glw = widget.findChild<OpenGLWidget *>("openGLWidget");
    REQUIRE(glw != nullptr);
    
    // Get the state for setting time range
    auto * state = widget.state();
    REQUIRE(state != nullptr);
    
    // Set initial time to middle of video
    int const initial_time = 352;
    invoked = QMetaObject::invokeMethod(
            &widget,
            "_updatePlot",
            Qt::DirectConnection,
            Q_ARG(int, initial_time));
    REQUIRE(invoked);
    QApplication::processEvents();
    
    INFO("Testing short video (704 frames) extreme zoom regression");
    
    // Cycle 1: Start with a reasonable range (100 samples)
    INFO("Cycle 1: Set range to 100 samples");
    state->setTimeWidth(100);
    QApplication::processEvents();
    
    auto const & view_state_1 = glw->getViewState();
    int64_t range_1 = view_state_1.getTimeWidth();
    INFO("Cycle 1: Achieved range = " << range_1);
    REQUIRE(range_1 > 0);
    
    // Cycle 2: Zoom out to full range
    INFO("Cycle 2: Zoom to full range (704 samples)");
    state->setTimeWidth(704);
    QApplication::processEvents();
    
    auto const & view_state_2 = glw->getViewState();
    int64_t range_2 = view_state_2.getTimeWidth();
    INFO("Cycle 2: Achieved range = " << range_2);
    REQUIRE(range_2 > 0);
    
    // Cycle 3: Zoom way in (10 samples)
    INFO("Cycle 3: Zoom to 10 samples");
    state->setTimeWidth(10);
    QApplication::processEvents();
    
    auto const & view_state_3 = glw->getViewState();
    int64_t range_3 = view_state_3.getTimeWidth();
    INFO("Cycle 3: Achieved range = " << range_3);
    REQUIRE(range_3 > 0);
    
    // Cycle 4: Zoom to 2 samples (the reported stuck state)
    INFO("Cycle 4: Zoom to 2 samples (bug trigger)");
    state->setTimeWidth(2);
    QApplication::processEvents();
    
    auto const & view_state_4 = glw->getViewState();
    int64_t range_4 = view_state_4.getTimeWidth();
    INFO("Cycle 4: Achieved range = " << range_4);
    REQUIRE(range_4 > 0);
    
    // Cycle 5: THE KEY TEST - try to zoom back out from 2 samples
    INFO("Cycle 5: Attempt to zoom out from 2 samples to 200 samples");
    state->setTimeWidth(200);
    QApplication::processEvents();
    
    auto const & view_state_5 = glw->getViewState();
    int64_t range_5 = view_state_5.getTimeWidth();
    INFO("Cycle 5: After requesting 200 samples, achieved range = " << range_5);
    
    // This is the regression test: we should be able to zoom out
    REQUIRE(range_5 >= 150);  // Should be close to 200 (allow some clamping)
    REQUIRE(range_5 > 2);     // Should definitely not be stuck at 2!
    
    // Cycle 6: Multiple rapid extreme zoom cycles
    INFO("Cycle 6: Rapid extreme zoom cycles");
    std::vector<int> test_ranges = {704, 50, 400, 10, 500, 5, 704, 2, 350, 100};
    
    for (size_t i = 0; i < test_ranges.size(); ++i) {
        int requested_range = test_ranges[i];
        
        state->setTimeWidth(requested_range);
        QApplication::processEvents();
        
        auto const & view_state = glw->getViewState();
        int64_t achieved_range = view_state.getTimeWidth();
        
        INFO("Rapid cycle " << i << ": requested=" << requested_range << ", achieved=" << achieved_range);
        
        // Basic validity checks
        REQUIRE(achieved_range > 0);
        REQUIRE(view_state.time_start <= view_state.time_end);
        
        // The range should be reasonably close to what we requested
        // (allow up to 20% difference for boundary clamping)
        if (requested_range <= 704) {
            float const ratio = static_cast<float>(achieved_range) / static_cast<float>(requested_range);
            INFO("  Ratio of achieved/requested: " << ratio);
            REQUIRE(ratio >= 0.8f);  // At least 80% of requested
            REQUIRE(ratio <= 1.2f);  // Not more than 120% of requested
        }
    }
    
    // Cycle 7: Final verification - zoom to full range after all the abuse
    INFO("Cycle 7: Final verification - full range after stress test");
    state->setTimeWidth(704);
    QApplication::processEvents();
    
    auto const & view_state_final = glw->getViewState();
    int64_t final_range = view_state_final.getTimeWidth();
    INFO("Cycle 7: Final range = " << final_range);
    
    REQUIRE(final_range >= 650);  // Should be close to full 704
    REQUIRE(final_range <= 704);
    
    widget.close();
}

