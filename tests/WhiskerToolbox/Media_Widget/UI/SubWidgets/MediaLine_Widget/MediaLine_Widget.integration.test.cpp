/**
 * @file MediaLine_Widget.integration.test.cpp
 * @brief Integration tests for MediaLine_Widget line editing functionality
 * 
 * These tests verify that:
 * 1. Points can be added to a line by clicking in the media widget
 */

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/MediaWidgetRegistration.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "Media_Widget/UI/Media_Widget.hpp"
#include "Media_Widget/UI/MediaPropertiesWidget.hpp"
#include "Media_Widget/UI/SubWidgets/MediaLine_Widget/MediaLine_Widget.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <QApplication>
#include <QComboBox>
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
 * @brief Helper to create a DataManager with a TimeFrame and LineData
 */
std::shared_ptr<DataManager> createDataManagerWithLine(
        std::string const & line_key,
        int num_frames,
        ImageSize const & image_size) {
    
    auto dm = std::make_shared<DataManager>();

    // Create timeframe
    std::vector<int> t(num_frames);
    std::iota(t.begin(), t.end(), 0);
    auto tf = std::make_shared<TimeFrame>(t);
    dm->setTime(TimeKey("time"), tf, true);

    // Create line data
    dm->setData<LineData>(line_key, TimeKey("time"));
    auto line_data = dm->getData<LineData>(line_key);
    if (line_data) {
        line_data->setImageSize(image_size);
    }

    return dm;
}

/**
 * @brief Helper to select a line feature in MediaPropertiesWidget
 * @return Pointer to the MediaLine_Widget after selection
 */
MediaLine_Widget * selectLineFeature(
        MediaPropertiesWidget & widget,
        std::string const & line_key,
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

    // Find line row
    int line_row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        auto * item = table->item(r, featureColumnIndex);
        if (item && item->text() == QString::fromStdString(line_key)) {
            line_row = r;
            break;
        }
    }
    if (line_row < 0) return nullptr;

    // Select the feature
    QMetaObject::invokeMethod(
            feature_table,
            "_highlightFeature",
            Qt::DirectConnection,
            Q_ARG(int, line_row),
            Q_ARG(int, featureColumnIndex));
    app->processEvents();

    // Get the line widget from stacked widget (index 2 for lines)
    auto stack = widget.findChild<QStackedWidget *>("stackedWidget");
    if (!stack || stack->currentIndex() != 2) return nullptr;

    auto line_widget = qobject_cast<MediaLine_Widget *>(stack->widget(2));
    if (line_widget) {
        line_widget->show();
        app->processEvents();
    }
    return line_widget;
}

/**
 * @brief Enable "Select Line" mode on a MediaLine_Widget
 */
void enableSelectLineMode(MediaLine_Widget * line_widget, QApplication * app) {
    auto combo = line_widget->findChild<QComboBox *>("selection_mode_combo");
    if (combo) {
        combo->setCurrentText("Select Line");
        app->processEvents();
    }
}

/**
 * @brief Simulate a click with modifiers on a MediaLine_Widget
 * 
 * @param line_widget The MediaLine_Widget to interact with
 * @param x_media X coordinate in media space
 * @param y_media Y coordinate in media space
 * @param modifiers Keyboard modifiers (Qt::ControlModifier for add, Qt::AltModifier for erase)
 */
void simulateLineClick(MediaLine_Widget * line_widget,
                       qreal x_media,
                       qreal y_media,
                       Qt::KeyboardModifiers modifiers) {
    if (!line_widget) return;

    QMetaObject::invokeMethod(line_widget, "_clickedInVideoWithModifiers", 
                              Qt::DirectConnection,
                              Q_ARG(qreal, x_media),
                              Q_ARG(qreal, y_media),
                              Q_ARG(Qt::KeyboardModifiers, modifiers));
}

}  // namespace

// ============================================================================
// Line Point Addition Tests
// ============================================================================

TEST_CASE("Points can be added to a line by clicking in media widget",
          "[MediaWidget][MediaLine_Widget][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 50;

    auto data_manager = createDataManagerWithLine("test_line", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    // Pre-create a line at the target frame so we can select it
    auto line_data = data_manager->getData<LineData>("test_line");
    REQUIRE(line_data != nullptr);
    
    // Create an initial line with a few points
    Line2D initial_line({
        Point2D<float>{100.0f, 100.0f},
        Point2D<float>{120.0f, 120.0f}
    });
    line_data->addAtTime(TimeFrameIndex{kTargetFrame}, initial_line, NotifyObservers::No);
    
    // Get the EntityId of the line we just created
    auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex{kTargetFrame});
    REQUIRE_FALSE(entity_ids.empty());
    EntityId line_entity_id = entity_ids[0];

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    // Set the current position to the target frame
    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    state->current_position = position;
    REQUIRE(state->current_position.isValid());
    REQUIRE(state->current_position.index.getValue() == kTargetFrame);

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto line_widget = selectLineFeature(props_widget, "test_line", app);
        REQUIRE(line_widget != nullptr);

        // Enable "Select Line" mode
        enableSelectLineMode(line_widget, app);

        // Select the line using the scene's selectEntity method
        media_window->selectEntity(line_entity_id, "test_line", "line");
        app->processEvents();

        // Verify line is selected
        auto selected_entities = media_window->getSelectedEntities();
        REQUIRE_FALSE(selected_entities.empty());
        REQUIRE(selected_entities.count(line_entity_id) > 0);

        // Get initial line point count
        auto line_ref = line_data->getDataByEntityId(line_entity_id);
        REQUIRE(line_ref.has_value());
        size_t initial_point_count = line_ref.value().get().size();
        REQUIRE(initial_point_count == 2);

        // Simulate Ctrl+click to add a point (Ctrl modifier is used for adding points)
        constexpr qreal kClickX = 140.0;
        constexpr qreal kClickY = 140.0;
        simulateLineClick(line_widget, kClickX, kClickY, Qt::ControlModifier);
        app->processEvents();

        // Verify point was added to the line
        auto line_ref_after = line_data->getDataByEntityId(line_entity_id);
        REQUIRE(line_ref_after.has_value());
        size_t final_point_count = line_ref_after.value().get().size();
        REQUIRE(final_point_count > initial_point_count);
        REQUIRE(final_point_count == 3);

        // Verify the new point is at the correct location
        auto const & line = line_ref_after.value().get();
        auto const & last_point = line.back();
        REQUIRE(last_point.x == Catch::Approx(kClickX));
        REQUIRE(last_point.y == Catch::Approx(kClickY));
    }
}

TEST_CASE("Multiple points can be added to a line",
          "[MediaWidget][MediaLine_Widget][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 30;

    auto data_manager = createDataManagerWithLine("test_line", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    // Pre-create a line
    auto line_data = data_manager->getData<LineData>("test_line");
    REQUIRE(line_data != nullptr);
    
    Line2D initial_line({
        Point2D<float>{50.0f, 50.0f},
        Point2D<float>{60.0f, 60.0f}
    });
    line_data->addAtTime(TimeFrameIndex{kTargetFrame}, initial_line, NotifyObservers::No);
    
    auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex{kTargetFrame});
    REQUIRE_FALSE(entity_ids.empty());
    EntityId line_entity_id = entity_ids[0];

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    state->current_position = position;

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto line_widget = selectLineFeature(props_widget, "test_line", app);
        REQUIRE(line_widget != nullptr);

        enableSelectLineMode(line_widget, app);
        media_window->selectEntity(line_entity_id, "test_line", "line");
        app->processEvents();

        // Add first point
        simulateLineClick(line_widget, 70.0, 70.0, Qt::ControlModifier);
        app->processEvents();

        // Add second point
        simulateLineClick(line_widget, 80.0, 80.0, Qt::ControlModifier);
        app->processEvents();

        // Add third point
        simulateLineClick(line_widget, 90.0, 90.0, Qt::ControlModifier);
        app->processEvents();

        // Verify all three points were added
        auto line_ref = line_data->getDataByEntityId(line_entity_id);
        REQUIRE(line_ref.has_value());
        REQUIRE(line_ref.value().get().size() == 5); // 2 initial + 3 new
    }
}

TEST_CASE("Adding points to line works at correct time frame",
          "[MediaWidget][MediaLine_Widget][Time][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kFrame1 = 20;
    constexpr int kFrame2 = 60;

    auto data_manager = createDataManagerWithLine("test_line", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    // Pre-create lines at different frames
    auto line_data = data_manager->getData<LineData>("test_line");
    REQUIRE(line_data != nullptr);
    
    Line2D line1({
        Point2D<float>{100.0f, 100.0f},
        Point2D<float>{110.0f, 110.0f}
    });
    line_data->addAtTime(TimeFrameIndex{kFrame1}, line1, NotifyObservers::No);
    
    Line2D line2({
        Point2D<float>{200.0f, 200.0f},
        Point2D<float>{210.0f, 210.0f}
    });
    line_data->addAtTime(TimeFrameIndex{kFrame2}, line2, NotifyObservers::No);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto line_widget = selectLineFeature(props_widget, "test_line", app);
        REQUIRE(line_widget != nullptr);

        enableSelectLineMode(line_widget, app);

        // Add point to line at frame 20
        state->current_position = TimePosition(TimeFrameIndex{kFrame1}, time_frame);
        auto entity_ids_frame1 = line_data->getEntityIdsAtTime(TimeFrameIndex{kFrame1});
        REQUIRE_FALSE(entity_ids_frame1.empty());
        media_window->selectEntity(entity_ids_frame1[0], "test_line", "line");
        app->processEvents();
        
        simulateLineClick(line_widget, 120.0, 120.0, Qt::ControlModifier);
        app->processEvents();

        // Add point to line at frame 60
        state->current_position = TimePosition(TimeFrameIndex{kFrame2}, time_frame);
        auto entity_ids_frame2 = line_data->getEntityIdsAtTime(TimeFrameIndex{kFrame2});
        REQUIRE_FALSE(entity_ids_frame2.empty());
        media_window->selectEntity(entity_ids_frame2[0], "test_line", "line");
        app->processEvents();
        
        simulateLineClick(line_widget, 220.0, 220.0, Qt::ControlModifier);
        app->processEvents();

        // Verify both lines have the correct point counts
        auto line1_ref = line_data->getDataByEntityId(entity_ids_frame1[0]);
        REQUIRE(line1_ref.has_value());
        REQUIRE(line1_ref.value().get().size() == 3); // 2 initial + 1 new

        auto line2_ref = line_data->getDataByEntityId(entity_ids_frame2[0]);
        REQUIRE(line2_ref.has_value());
        REQUIRE(line2_ref.value().get().size() == 3); // 2 initial + 1 new
    }
}

// ============================================================================
// Full Integration Test
// ============================================================================

TEST_CASE("Full integration: EditorRegistry creation with line point addition",
          "[MediaWidget][MediaLine_Widget][EditorRegistry][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<qreal>("qreal");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 42;

    auto data_manager = createDataManagerWithLine("test_line", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));

    // Pre-create a line
    auto line_data = data_manager->getData<LineData>("test_line");
    REQUIRE(line_data != nullptr);
    
    Line2D initial_line({
        Point2D<float>{150.0f, 150.0f},
        Point2D<float>{160.0f, 160.0f}
    });
    line_data->addAtTime(TimeFrameIndex{kTargetFrame}, initial_line, NotifyObservers::No);
    
    auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex{kTargetFrame});
    REQUIRE_FALSE(entity_ids.empty());
    EntityId line_entity_id = entity_ids[0];

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

    // Now verify that line editing uses this frame
    props->resize(900, 700);
    props->show();
    app->processEvents();

    auto line_widget = selectLineFeature(*props, "test_line", app);
    REQUIRE(line_widget != nullptr);

    enableSelectLineMode(line_widget, app);
    
    // Get Media_Window from the view
    auto * media_window = view->getMediaWindow();
    REQUIRE(media_window != nullptr);
    
    // Select the line
    media_window->selectEntity(line_entity_id, "test_line", "line");
    app->processEvents();

    // Add a point
    simulateLineClick(line_widget, 170.0, 170.0, Qt::ControlModifier);
    app->processEvents();

    // Verify point was added at frame 42
    auto line_ref = line_data->getDataByEntityId(line_entity_id);
    REQUIRE(line_ref.has_value());
    REQUIRE(line_ref.value().get().size() == 3); // 2 initial + 1 new
    
    auto const & line = line_ref.value().get();
    auto const & last_point = line.back();
    REQUIRE(last_point.x == Catch::Approx(170.0f));
    REQUIRE(last_point.y == Catch::Approx(170.0f));
}
