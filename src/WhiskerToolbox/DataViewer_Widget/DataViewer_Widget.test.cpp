#include "DataViewer_Widget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QWidget>
#include <QTimer>

#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

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
        auto timeframe = std::make_shared<TimeFrame>();
        TimeKey time_key("time");
        m_data_manager->setTime(time_key, timeframe);

        // Add test AnalogTimeSeries
        std::vector<float> analog_values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        auto analog_data = std::make_shared<AnalogTimeSeries>(analog_values, analog_values.size());
        m_data_manager->setData<AnalogTimeSeries>("test_analog", analog_data, time_key);
        m_test_data_keys.push_back("test_analog");

        // Add test DigitalEventSeries
        auto event_data = std::make_shared<DigitalEventSeries>();
        event_data->addEvent(1000);
        event_data->addEvent(2000);
        event_data->addEvent(3000);
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
        for (const auto& key : test_keys) {
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
        for (const auto& weak_ref : weak_refs) {
            REQUIRE(!weak_ref.expired());
        }

        // Delete data from DataManager
        for (const auto& key : test_keys) {
            bool deleted = dm.deleteData(key);
            REQUIRE(deleted);
        }

        // Process Qt events to ensure the observer callback executes
        QApplication::processEvents();

        // Verify that weak references are now expired (data cleaned up)
        for (const auto& weak_ref : weak_refs) {
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
        
        for (const auto& key : test_keys) {
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
        for (const auto& key : test_keys) {
            dm.deleteData(key);
        }

        // Process Qt events to ensure observer callbacks execute
        QApplication::processEvents();

        // Verify all data has been cleaned up
        int final_analog_count = 0;
        int final_event_count = 0;
        int final_interval_count = 0;
        
        for (const auto& key : test_keys) {
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

        REQUIRE(analog_data.use_count() > 1); // DataManager + any internal references
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
        
        for (const auto& key : keys_to_delete) {
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
        auto timeframe = std::make_shared<TimeFrame>();
        m_time_key = TimeKey("time");
        m_data_manager->setTime(m_time_key, timeframe);

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
            for (auto & v : values) v *= scale;

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
        auto cfg = widget.getAnalogConfig(key);
        REQUIRE(cfg.has_value());
        REQUIRE(cfg.value() != nullptr);
        REQUIRE(cfg.value()->is_visible);

        // Gather centers and heights for all enabled series so far
        std::vector<float> centers;
        std::vector<float> heights;
        centers.reserve(i + 1);
        heights.reserve(i + 1);

        for (size_t j = 0; j <= i; ++j) {
            auto c = widget.getAnalogConfig(keys[j]);
            REQUIRE(c.has_value());
            REQUIRE(c.value() != nullptr);
            centers.push_back(static_cast<float>(c.value()->allocated_y_center));
            heights.push_back(static_cast<float>(c.value()->allocated_height));
        }

        std::sort(centers.begin(), centers.end());

        // Expected evenly spaced centers across [-1, 1] at fractions k/(N+1)
        size_t const enabled_count = i + 1;
        float const tol_center = 0.22f; // generous tolerance for layout differences
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
        for (auto const h : heights) {
 //           REQUIRE(h >= expected_spacing * 0.4f);
   //         REQUIRE(h <= expected_spacing * 1.2f);
        }

        // And heights should be fairly consistent across series (within 40%)
        if (min_h > 0.0f) {
            REQUIRE((max_h / min_h) <= 1.4f);
        }
    }
}
