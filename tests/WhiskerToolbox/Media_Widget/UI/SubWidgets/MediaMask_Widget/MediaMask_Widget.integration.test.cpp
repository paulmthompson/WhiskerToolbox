/**
 * @file MediaMask_Widget.integration.test.cpp
 * @brief Integration tests for MediaMask_Widget state sharing and time-aware mask editing
 * 
 * These tests verify that:
 * 1. MediaMask_Widget shares the same state instance as Media_Widget and MediaPropertiesWidget
 * 2. Mask pixels are added at the correct time position (not always frame 0)
 * 
 * Bug context: A previous bug caused MediaMask_Widget to receive a different state
 * instance than Media_Widget. When Media_Widget::LoadFrame() updated current_position
 * in its state, MediaMask_Widget's state remained at frame 0, causing all mask edits
 * to go to frame 0 regardless of the displayed frame.
 */

#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/DisplayOptions/CoordinateTypes.hpp"
#include "Media_Widget/MediaWidgetRegistration.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "Media_Widget/UI/Media_Widget.hpp"
#include "Media_Widget/UI/MediaPropertiesWidget.hpp"
#include "Media_Widget/UI/SubWidgets/MediaMask_Widget/MediaMask_Widget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

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
 * @brief Helper to create a DataManager with a TimeFrame and MaskData
 */
std::shared_ptr<DataManager> createDataManagerWithMask(
        std::string const & mask_key,
        int num_frames,
        ImageSize const & mask_size) {
    
    auto dm = std::make_shared<DataManager>();

    // Create timeframe
    std::vector<int> t(num_frames);
    std::iota(t.begin(), t.end(), 0);
    auto tf = std::make_shared<TimeFrame>(t);
    dm->setTime(TimeKey("time"), tf, true);

    // Create mask data
    auto mask = std::make_shared<MaskData>();
    mask->setImageSize(mask_size);
    dm->setData<MaskData>(mask_key, mask, TimeKey("time"));

    return dm;
}

/**
 * @brief Helper to select a mask feature in MediaPropertiesWidget
 * @return Pointer to the MediaMask_Widget after selection
 */
MediaMask_Widget * selectMaskFeature(
        MediaPropertiesWidget & widget,
        std::string const & mask_key,
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

    // Find mask row
    int mask_row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        auto * item = table->item(r, featureColumnIndex);
        if (item && item->text() == QString::fromStdString(mask_key)) {
            mask_row = r;
            break;
        }
    }
    if (mask_row < 0) return nullptr;

    // Select the feature
    QMetaObject::invokeMethod(
            feature_table,
            "_highlightFeature",
            Qt::DirectConnection,
            Q_ARG(int, mask_row),
            Q_ARG(int, featureColumnIndex));
    app->processEvents();

    // Get the mask widget from stacked widget
    auto stack = widget.findChild<QStackedWidget *>("stackedWidget");
    if (!stack || stack->currentIndex() != 3) return nullptr;

    auto mask_widget = qobject_cast<MediaMask_Widget *>(stack->widget(3));
    if (mask_widget) {
        mask_widget->show();
        app->processEvents();
    }
    return mask_widget;
}

/**
 * @brief Enable brush mode on a MediaMask_Widget
 */
void enableBrushMode(MediaMask_Widget * mask_widget, QApplication * app) {
    auto combo = mask_widget->findChild<QComboBox *>("selection_mode_combo");
    if (combo) {
        combo->setCurrentText("Brush");
        app->processEvents();
    }
}

/**
 * @brief Simulate a brush stroke on a MediaMask_Widget
 */
void simulateBrushStroke(MediaMask_Widget * mask_widget, 
                          std::vector<CanvasCoordinates> const & points) {
    if (points.empty()) return;

    // First point is a click
    QMetaObject::invokeMethod(mask_widget, "_clickedInVideo", Qt::DirectConnection,
                              Q_ARG(CanvasCoordinates, points[0]));

    // Subsequent points are moves
    for (size_t i = 1; i < points.size(); ++i) {
        QMetaObject::invokeMethod(mask_widget, "_mouseMoveInVideo", Qt::DirectConnection,
                                  Q_ARG(CanvasCoordinates, points[i]));
    }

    // Release
    QMetaObject::invokeMethod(mask_widget, "_mouseReleased", Qt::DirectConnection);
}

}  // namespace

// ============================================================================
// State Sharing Tests
// ============================================================================

TEST_CASE("MediaWidget components share the same state instance via EditorRegistry",
          "[MediaWidget][State][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    auto data_manager = createDataManagerWithMask("test_mask", 100, {640, 480});
    
    // Create EditorRegistry and register MediaWidget type
    EditorRegistry registry(nullptr);
    MediaWidgetModule::registerTypes(&registry, data_manager, nullptr);

    SECTION("createEditor returns state that matches view's internal state") {
        auto instance = registry.createEditor(EditorLib::EditorTypeId("MediaWidget"));

        REQUIRE(instance.state != nullptr);
        REQUIRE(instance.view != nullptr);

        auto * view = qobject_cast<Media_Widget *>(instance.view);
        REQUIRE(view != nullptr);

        // THE KEY TEST: The state returned by createEditor must be the same
        // instance that the view widget uses internally
        auto view_state = view->getState();
        REQUIRE(view_state.get() == instance.state.get());
    }

    SECTION("View and Properties widgets use the same state instance") {
        auto instance = registry.createEditor(EditorLib::EditorTypeId("MediaWidget"));

        REQUIRE(instance.state != nullptr);
        REQUIRE(instance.view != nullptr);
        REQUIRE(instance.properties != nullptr);

        auto * view = qobject_cast<Media_Widget *>(instance.view);
        auto * props = qobject_cast<MediaPropertiesWidget *>(instance.properties);

        REQUIRE(view != nullptr);
        REQUIRE(props != nullptr);

        // Both should reference the same state
        auto view_state = view->getState();
        REQUIRE(view_state.get() == instance.state.get());

        // Cast to MediaWidgetState to verify type
        auto media_state = std::dynamic_pointer_cast<MediaWidgetState>(instance.state);
        REQUIRE(media_state != nullptr);
    }
}

TEST_CASE("MediaMask_Widget state initial current_position is invalid",
          "[MediaWidget][MediaMask_Widget][State][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<CanvasCoordinates>("CanvasCoordinates");

    auto data_manager = createDataManagerWithMask("test_mask", 200, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    auto state = std::make_shared<MediaWidgetState>();
    
    // Use unique_ptr for proper RAII and destruction order control
    auto media_window = std::make_unique<Media_Window>(data_manager);

    // Use a scope block to ensure props_widget is destroyed before media_window
    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto mask_widget = selectMaskFeature(props_widget, "test_mask", app);
        REQUIRE(mask_widget != nullptr);

        // Before any LoadFrame call, current_position should be default
        REQUIRE_FALSE(state->current_position.isValid());
    }
    // props_widget destroyed here, then media_window destroyed when unique_ptr goes out of scope
}

TEST_CASE("Setting current_position on state is visible to MediaMask_Widget operations",
          "[MediaWidget][MediaMask_Widget][State][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<CanvasCoordinates>("CanvasCoordinates");

    auto data_manager = createDataManagerWithMask("test_mask", 200, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto mask_widget = selectMaskFeature(props_widget, "test_mask", app);
        REQUIRE(mask_widget != nullptr);

        // Simulate what Media_Widget::LoadFrame does
        TimeFrameIndex const target_frame{50};
        TimePosition position(target_frame, time_frame);
        state->current_position = position;

        REQUIRE(state->current_position.isValid());
        REQUIRE(state->current_position.index == target_frame);

        // Enable brush mode and simulate a stroke
        enableBrushMode(mask_widget, app);
        simulateBrushStroke(mask_widget, {
            {100.0f, 100.0f},
            {110.0f, 110.0f},
            {120.0f, 120.0f}
        });
        app->processEvents();

        // Verify mask was added at frame 50, not frame 0
        auto mask_data = data_manager->getData<MaskData>("test_mask");
        REQUIRE(mask_data != nullptr);

        // Check frame 50 has mask data
        auto frame50_idx = TimeIndexAndFrame(target_frame, time_frame.get());
        auto const & masks_at_50 = mask_data->getAtTime(frame50_idx);
        REQUIRE_FALSE(masks_at_50.empty());
        REQUIRE_FALSE(masks_at_50[0].empty());

        // Check frame 0 does NOT have mask data (this was the bug)
        auto frame0_idx = TimeIndexAndFrame(TimeFrameIndex{0}, time_frame.get());
        auto const & masks_at_0 = mask_data->getAtTime(frame0_idx);
        REQUIRE(masks_at_0.empty());
    }
}

// ============================================================================
// Time-Aware Mask Editing Tests
// ============================================================================

TEST_CASE("Adding mask pixels goes to current frame, not frame 0",
          "[MediaMask_Widget][Mask][Time][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<CanvasCoordinates>("CanvasCoordinates");

    constexpr int kNumFrames = 200;
    constexpr int kTargetFrame = 75;

    auto data_manager = createDataManagerWithMask("test_mask", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    // Set the current position to a non-zero frame BEFORE creating the widget
    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    state->current_position = position;
    REQUIRE(state->current_position.isValid());
    REQUIRE(state->current_position.index.getValue() == kTargetFrame);

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto mask_widget = selectMaskFeature(props_widget, "test_mask", app);
        REQUIRE(mask_widget != nullptr);

        enableBrushMode(mask_widget, app);

        simulateBrushStroke(mask_widget, {
            {200.0f, 200.0f},
            {210.0f, 210.0f}
        });
        app->processEvents();

        auto mask_data = data_manager->getData<MaskData>("test_mask");
        REQUIRE(mask_data != nullptr);

        // Verify mask was added at the target frame
        auto target_idx = TimeIndexAndFrame(TimeFrameIndex{kTargetFrame}, time_frame.get());
        auto const & masks_at_target = mask_data->getAtTime(target_idx);
        INFO("Expected mask at frame " << kTargetFrame);
        REQUIRE_FALSE(masks_at_target.empty());
        REQUIRE_FALSE(masks_at_target[0].empty());

        // Verify frame 0 is empty
        auto frame0_idx = TimeIndexAndFrame(TimeFrameIndex{0}, time_frame.get());
        auto const & masks_at_0 = mask_data->getAtTime(frame0_idx);
        INFO("Frame 0 should be empty");
        REQUIRE(masks_at_0.empty());
    }
}

TEST_CASE("Changing time position affects where new mask pixels are added",
          "[MediaMask_Widget][Mask][Time][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<CanvasCoordinates>("CanvasCoordinates");

    constexpr int kNumFrames = 200;
    constexpr int kTargetFrame = 75;

    auto data_manager = createDataManagerWithMask("test_mask", kNumFrames, {640, 480});
    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);

    auto state = std::make_shared<MediaWidgetState>();
    auto media_window = std::make_unique<Media_Window>(data_manager);

    // Set the current position to frame 75
    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    state->current_position = position;

    {
        MediaPropertiesWidget props_widget(state, data_manager, media_window.get());
        props_widget.resize(900, 700);
        props_widget.show();
        app->processEvents();

        auto mask_widget = selectMaskFeature(props_widget, "test_mask", app);
        REQUIRE(mask_widget != nullptr);

        enableBrushMode(mask_widget, app);

        // First, add some pixels at frame 75
        simulateBrushStroke(mask_widget, {
            {100.0f, 100.0f}
        });
        app->processEvents();

        // Change to frame 150
        constexpr int kSecondFrame = 150;
        state->current_position = TimePosition(TimeFrameIndex{kSecondFrame}, time_frame);
        REQUIRE(state->current_position.index.getValue() == kSecondFrame);

        // Add more pixels - these should go to frame 150
        simulateBrushStroke(mask_widget, {
            {300.0f, 300.0f}
        });
        app->processEvents();

        auto mask_data = data_manager->getData<MaskData>("test_mask");

        // Frame 75 should have mask from first stroke
        auto frame75_idx = TimeIndexAndFrame(TimeFrameIndex{kTargetFrame}, time_frame.get());
        auto const & masks_at_75 = mask_data->getAtTime(frame75_idx);
        REQUIRE_FALSE(masks_at_75.empty());

        // Frame 150 should have mask from second stroke
        auto frame150_idx = TimeIndexAndFrame(TimeFrameIndex{kSecondFrame}, time_frame.get());
        auto const & masks_at_150 = mask_data->getAtTime(frame150_idx);
        REQUIRE_FALSE(masks_at_150.empty());

        // Frame 0 should still be empty
        auto frame0_idx = TimeIndexAndFrame(TimeFrameIndex{0}, time_frame.get());
        auto const & masks_at_0 = mask_data->getAtTime(frame0_idx);
        REQUIRE(masks_at_0.empty());
    }
}

TEST_CASE("Full integration: EditorRegistry creation with mask editing at non-zero frame",
          "[MediaWidget][MediaMask_Widget][EditorRegistry][Integration]") {
    auto * app = ensureQApplication();
    REQUIRE(app != nullptr);

    qRegisterMetaType<CanvasCoordinates>("CanvasCoordinates");

    constexpr int kNumFrames = 100;
    constexpr int kTargetFrame = 42;

    auto data_manager = createDataManagerWithMask("test_mask", kNumFrames, {640, 480});
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

    // Simulate loading frame 42 (what happens when user scrolls the time bar)
    TimePosition position(TimeFrameIndex{kTargetFrame}, time_frame);
    view->LoadFrame(position);
    app->processEvents();

    // Verify state was updated
    auto media_state = std::dynamic_pointer_cast<MediaWidgetState>(instance.state);
    REQUIRE(media_state != nullptr);
    REQUIRE(media_state->current_position.isValid());
    REQUIRE(media_state->current_position.index.getValue() == kTargetFrame);

    // Now verify that mask editing uses this frame
    props->resize(900, 700);
    props->show();
    app->processEvents();

    auto mask_widget = selectMaskFeature(*props, "test_mask", app);
    REQUIRE(mask_widget != nullptr);

    enableBrushMode(mask_widget, app);

    simulateBrushStroke(mask_widget, {
        {150.0f, 150.0f},
        {160.0f, 160.0f}
    });
    app->processEvents();

    // Verify mask was added at frame 42
    auto mask_data = data_manager->getData<MaskData>("test_mask");
    REQUIRE(mask_data != nullptr);

    auto target_idx = TimeIndexAndFrame(TimeFrameIndex{kTargetFrame}, time_frame.get());
    auto const & masks_at_target = mask_data->getAtTime(target_idx);
    INFO("Mask should be at frame " << kTargetFrame << " after LoadFrame and brush stroke");
    REQUIRE_FALSE(masks_at_target.empty());
    REQUIRE_FALSE(masks_at_target[0].empty());

    // Frame 0 should be empty
    auto frame0_idx = TimeIndexAndFrame(TimeFrameIndex{0}, time_frame.get());
    auto const & masks_at_0 = mask_data->getAtTime(frame0_idx);
    INFO("Frame 0 should remain empty - this was the original bug");
    REQUIRE(masks_at_0.empty());
}
