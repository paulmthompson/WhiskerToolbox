/**
 * @file MediaPoint_Widget.integration.test.cpp
 * @brief Integration tests for MediaPoint_Widget point editing functionality
 * 
 * These tests verify that:
 * 1. Points can be added by clicking in the media widget and they show up in the data manager
 * 2. The position of an existing point can be changed
 */

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/MediaWidgetRegistration.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "Media_Widget/UI/Media_Widget.hpp"
#include "Media_Widget/UI/MediaPropertiesWidget.hpp"
#include "Media_Widget/UI/SubWidgets/MediaPoint_Widget/MediaPoint_Widget.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/StrongTypes.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <QApplication>
#include <QStackedWidget>
#include <QTableWidget>

#include <array>
#include <memory>
#include <numeric>
#include <vector>

namespace {

/**
 * @brief Helper to ensure QApplication exists for GUI tests
 */
QApplication * ensureQApplication() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
    return qobject_cast<QApplication *>(QApplication::instance());
}

/**
 * @brief Helper to create a DataManager with a TimeFrame and PointData
 */
std::shared_ptr<DataManager> createDataManagerWithPoints(
        std::string const & point_key,
        int num_frames,
        ImageSize const & image_size) {
    
    auto dm = std::make_shared<DataManager>();

    // Create timeframe
    std::vector<int> t(num_frames);
    std::iota(t.begin(), t.end(), 0);
    auto tf = std::make_shared<TimeFrame>(t);
    dm->setTime(TimeKey("time"), tf, true);

    // Create point data
    dm->setData<PointData>(point_key, TimeKey("time"));
    auto point_data = dm->getData<PointData>(point_key);
    if (point_data) {
        point_data->setImageSize(image_size);
    }

    return dm;
}

/**
 * @brief Helper to select a point feature in MediaPropertiesWidget
 * @return Pointer to the MediaPoint_Widget after selection
 */
MediaPoint_Widget * selectPointFeature(
        MediaPropertiesWidget & widget,
        std::string const & point_key,
        QApplication * app) {
    
    auto feature_table = widget.findChild<Feature_Table_Widget *>("feature_table_widget");
    if (!feature_table) return nullptr;

    feature_table->populateTable();
    app->processEvents();

    auto table = feature_table->findChild<QTableWidget *>("available_features_table");
    if (!table) return nullptr;

    // Find feature column
    int featureColumnIndex = 0;
    for (int c = 0; c < table->columnCount(); ++c) {
        auto * headerItem = table->horizontalHeaderItem(c);
        if (headerItem && headerItem->text() == QString("Feature")) {
            featureColumnIndex = c;
            break;
        }
    }

    // Find point row
    int point_row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        auto * item = table->item(r, featureColumnIndex);
        if (item && item->text() == QString::fromStdString(point_key)) {
            point_row = r;
            break;
        }
    }
    if (point_row < 0) return nullptr;

    // Select the feature
    QMetaObject::invokeMethod(
            feature_table,
            "_highlightFeature",
            Qt::DirectConnection,
            Q_ARG(int, point_row),
            Q_ARG(int, featureColumnIndex));
    app->processEvents();

    // Get the point widget from stacked widget (index 1 for points)
    auto stack = widget.findChild<QStackedWidget *>("stackedWidget");
    if (!stack || stack->currentIndex() != 1) return nullptr;

    auto point_widget = qobject_cast<MediaPoint_Widget *>(stack->widget(1));
    if (point_widget) {
        point_widget->show();
        app->processEvents();
    }
    return point_widget;
}

/**
 * @brief Simulate a point click with modifiers on a MediaPoint_Widget
 * 
 * @param point_widget The MediaPoint_Widget to interact with
 * @param x_media X coordinate in media space
 * @param y_media Y coordinate in media space
 * @param modifiers Keyboard modifiers (Qt::AltModifier for add, Qt::ControlModifier for move)
 */
void simulatePointClick(MediaPoint_Widget * point_widget,
                        qreal x_media,
                        qreal y_media,
                        Qt::KeyboardModifiers modifiers) {
    if (!point_widget) return;

    // Get the Media_Window from the widget's scene
    // We need to invoke the slot that handles the click
    QMetaObject::invokeMethod(point_widget, "_handlePointClickWithModifiers", 
                              Qt::DirectConnection,
                              Q_ARG(qreal, x_media),
                              Q_ARG(qreal, y_media),
                              Q_ARG(Qt::KeyboardModifiers, modifiers));
}

}  // namespace

// ============================================================================
// Point Addition Tests
// ============================================================================

TEST_CASE("Points can be added by clicking in media widget and appear in data manager",
          "[MediaWidget][MediaPoint_Widget][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 50;

    auto data_manager = createDataManagerWithPoints("test_points", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    // Set the current position to a non-zero frame
    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    state->current_position = position;
    REQUIRE(state->current_position.isValid());
    REQUIRE(state->current_position.index.getValue() == kTargetFrame);

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto point_widget = selectPointFeature(props_widget, "test_points", app);
        REQUIRE(point_widget != nullptr);

        // Simulate Alt+click to add a point (Alt modifier is used for adding points)
        constexpr qreal kClickX = 200.0;
        constexpr qreal kClickY = 150.0;
        simulatePointClick(point_widget, kClickX, kClickY, Qt::AltModifier);
        app->processEvents();

        // Verify point was added at the target frame
        auto point_data = data_manager->getData<PointData>("test_points");
        REQUIRE(point_data != nullptr);

        auto target_idx = TimeIndexAndFrame(TimeFrameIndex{kTargetFrame}, time_frame.get());
        auto points_at_target = point_data->getAtTime(target_idx);
        REQUIRE_FALSE(points_at_target.empty());
        REQUIRE(points_at_target.size() == 1);
        REQUIRE(points_at_target[0].x == Catch::Approx(kClickX));
        REQUIRE(points_at_target[0].y == Catch::Approx(kClickY));

        // Verify frame 0 is empty
        auto frame0_idx = TimeIndexAndFrame(TimeFrameIndex{0}, time_frame.get());
        auto points_at_0 = point_data->getAtTime(frame0_idx);
        REQUIRE(points_at_0.empty());
    }
}

TEST_CASE("Multiple points can be added at the same frame",
          "[MediaWidget][MediaPoint_Widget][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 30;

    auto data_manager = createDataManagerWithPoints("test_points", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    state->current_position = position;

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto point_widget = selectPointFeature(props_widget, "test_points", app);
        REQUIRE(point_widget != nullptr);

        // Add first point
        simulatePointClick(point_widget, 100.0, 100.0, Qt::AltModifier);
        app->processEvents();

        // Add second point
        simulatePointClick(point_widget, 200.0, 200.0, Qt::AltModifier);
        app->processEvents();

        // Add third point
        simulatePointClick(point_widget, 300.0, 300.0, Qt::AltModifier);
        app->processEvents();

        // Verify all three points are at the target frame
        auto point_data = data_manager->getData<PointData>("test_points");
        REQUIRE(point_data != nullptr);

        auto target_idx = TimeIndexAndFrame(TimeFrameIndex{kTargetFrame}, time_frame.get());
        auto points_at_target = point_data->getAtTime(target_idx);
        REQUIRE(points_at_target.size() == 3);
    }
}

// ============================================================================
// Point Movement Tests
// ============================================================================

TEST_CASE("Position of existing point can be changed",
          "[MediaWidget][MediaPoint_Widget][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 25;

    auto data_manager = createDataManagerWithPoints("test_points", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    // Pre-add a point at the target frame
    auto point_data = data_manager->getData<PointData>("test_points");
    REQUIRE(point_data != nullptr);
    constexpr float kInitialX = 150.0f;
    constexpr float kInitialY = 175.0f;
    point_data->addAtTime(TimeFrameIndex{kTargetFrame}, 
                          Point2D<float>{kInitialX, kInitialY}, 
                          NotifyObservers::No);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    state->current_position = position;

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto point_widget = selectPointFeature(props_widget, "test_points", app);
        REQUIRE(point_widget != nullptr);

        // First, select the point by clicking near it (without modifiers)
        // The selection threshold is 10 pixels, so click within that range
        simulatePointClick(point_widget, kInitialX + 5.0, kInitialY + 5.0, Qt::NoModifier);
        app->processEvents();

        // Now move the point using Ctrl+click
        constexpr qreal kNewX = 250.0;
        constexpr qreal kNewY = 275.0;
        simulatePointClick(point_widget, kNewX, kNewY, Qt::ControlModifier);
        app->processEvents();

        // Verify point was moved
        auto target_idx = TimeIndexAndFrame(TimeFrameIndex{kTargetFrame}, time_frame.get());
        auto points_at_target = point_data->getAtTime(target_idx);
        REQUIRE_FALSE(points_at_target.empty());
        REQUIRE(points_at_target.size() == 1);
        REQUIRE(points_at_target[0].x == Catch::Approx(kNewX));
        REQUIRE(points_at_target[0].y == Catch::Approx(kNewY));
    }
}

TEST_CASE("Point movement works across different frames",
          "[MediaWidget][MediaPoint_Widget][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kFrame1 = 20;
    constexpr int kFrame2 = 60;

    auto data_manager = createDataManagerWithPoints("test_points", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    // Pre-add points at different frames
    auto point_data = data_manager->getData<PointData>("test_points");
    REQUIRE(point_data != nullptr);
    point_data->addAtTime(TimeFrameIndex{kFrame1}, 
                          Point2D<float>{100.0f, 100.0f}, 
                          NotifyObservers::No);
    point_data->addAtTime(TimeFrameIndex{kFrame2}, 
                          Point2D<float>{200.0f, 200.0f}, 
                          NotifyObservers::No);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto point_widget = selectPointFeature(props_widget, "test_points", app);
        REQUIRE(point_widget != nullptr);

        // Move point at frame 20
        state->current_position = TimePosition(TimeFrameIndex{kFrame1}, time_frame);
        simulatePointClick(point_widget, 105.0, 105.0, Qt::NoModifier);  // Select
        app->processEvents();
        simulatePointClick(point_widget, 150.0, 150.0, Qt::ControlModifier);  // Move
        app->processEvents();

        // Move point at frame 60
        state->current_position = TimePosition(TimeFrameIndex{kFrame2}, time_frame);
        simulatePointClick(point_widget, 205.0, 205.0, Qt::NoModifier);  // Select
        app->processEvents();
        simulatePointClick(point_widget, 300.0, 300.0, Qt::ControlModifier);  // Move
        app->processEvents();

        // Verify both points were moved correctly
        auto frame1_idx = TimeIndexAndFrame(TimeFrameIndex{kFrame1}, time_frame.get());
        auto points_at_frame1 = point_data->getAtTime(frame1_idx);
        REQUIRE(points_at_frame1.size() == 1);
        REQUIRE(points_at_frame1[0].x == Catch::Approx(150.0f));
        REQUIRE(points_at_frame1[0].y == Catch::Approx(150.0f));

        auto frame2_idx = TimeIndexAndFrame(TimeFrameIndex{kFrame2}, time_frame.get());
        auto points_at_frame2 = point_data->getAtTime(frame2_idx);
        REQUIRE(points_at_frame2.size() == 1);
        REQUIRE(points_at_frame2[0].x == Catch::Approx(300.0f));
        REQUIRE(points_at_frame2[0].y == Catch::Approx(300.0f));
    }
}

// ============================================================================
// Full Integration Test
// ============================================================================

TEST_CASE("Full integration: EditorRegistry creation with point editing",
          "[MediaWidget][MediaPoint_Widget][EditorRegistry][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 42;

    auto data_manager = createDataManagerWithPoints("test_points", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));

    // Use EditorRegistry to create the full widget setup
    EditorRegistry registry(nullptr);
    MediaWidgetModule::registerTypes(&registry, data_manager, nullptr);

    auto instance = registry.createEditor(EditorLib::EditorTypeId("MediaWidget"));
    REQUIRE(instance.state != nullptr);
    REQUIRE(instance.view != nullptr);
    REQUIRE(instance.properties != nullptr);

    auto * view = qobject_cast<Media_Widget *>(instance.view);
    auto * props = qobject_cast<MediaPropertiesWidget *>(instance.properties);
    REQUIRE(view != nullptr);
    REQUIRE(props != nullptr);

    // Verify state sharing
    REQUIRE(view->getState().get() == instance.state.get());

    // Simulate loading frame 42
    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    view->LoadFrame(position);
    app->processEvents();

    // Verify state was updated
    auto media_state = std::dynamic_pointer_cast<MediaWidgetState>(instance.state);
    REQUIRE(media_state != nullptr);
    REQUIRE(media_state->current_position.isValid());
    REQUIRE(media_state->current_position.index.getValue() == kTargetFrame);

    // Now verify that point editing uses this frame
    props->resize(900, 700);
    props->show();
    app->processEvents();

    auto point_widget = selectPointFeature(*props, "test_points", app);
    REQUIRE(point_widget != nullptr);

    // Add a point
    simulatePointClick(point_widget, 150.0, 150.0, Qt::AltModifier);
    app->processEvents();

    // Verify point was added at frame 42
    auto point_data = data_manager->getData<PointData>("test_points");
    REQUIRE(point_data != nullptr);

    auto target_idx = TimeIndexAndFrame(TimeFrameIndex{kTargetFrame}, time_frame.get());
    auto points_at_target = point_data->getAtTime(target_idx);
    REQUIRE_FALSE(points_at_target.empty());
    REQUIRE(points_at_target.size() == 1);
    REQUIRE(points_at_target[0].x == Catch::Approx(150.0f));
    REQUIRE(points_at_target[0].y == Catch::Approx(150.0f));

    // Frame 0 should be empty
    auto frame0_idx = TimeIndexAndFrame(TimeFrameIndex{0}, time_frame.get());
    auto points_at_0 = point_data->getAtTime(frame0_idx);
    REQUIRE(points_at_0.empty());
}
