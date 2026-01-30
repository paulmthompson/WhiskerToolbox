
#include "Core/MediaWidgetState.hpp"
#include "Media_Widget/UI/SubWidgets/MediaText_Widget/MediaText_Widget.hpp"
#include "Media_Widget/UI/SubWidgets/MediaMask_Widget/MediaMask_Widget.hpp"
#include "Media_Widget/DisplayOptions/CoordinateTypes.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "UI/MediaPropertiesWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QMetaObject>
#include <QMetaType>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>

#include <array>
#include <memory>
#include <numeric>
#include <vector>

// === MediaPropertiesWidget Tests ===

TEST_CASE("MediaPropertiesWidget construction", "[MediaPropertiesWidget]") {
    // Ensure a Qt application exists (same pattern as Media_Widget.test.cpp)
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with state and data manager") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        REQUIRE(widget.getMediaWindow() == nullptr);
    }

    SECTION("Constructs with nullptr state") {
        auto dm = std::make_shared<DataManager>();

        // Should not crash with nullptr state
        MediaPropertiesWidget widget(nullptr, dm);

        REQUIRE(widget.getMediaWindow() == nullptr);
    }

    SECTION("Constructs with nullptr data manager") {
        auto state = std::make_shared<MediaWidgetState>();

        // Should not crash with nullptr data manager
        MediaPropertiesWidget widget(state, nullptr);

        REQUIRE(widget.getMediaWindow() == nullptr);
    }
}

TEST_CASE("MediaPropertiesWidget setMediaWindow", "[MediaPropertiesWidget]") {
    // Ensure a Qt application exists
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    SECTION("Can set and get Media_Window") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Initially nullptr
        REQUIRE(widget.getMediaWindow() == nullptr);

        // Note: We can't easily create a real Media_Window in tests
        // but we can verify the setter/getter work with nullptr
        widget.setMediaWindow(nullptr);
        REQUIRE(widget.getMediaWindow() == nullptr);
    }
}

TEST_CASE("MediaPropertiesWidget has expected UI", "[MediaPropertiesWidget]") {
    // Ensure a Qt application exists
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    SECTION("Contains scroll area") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Find the scroll area
        auto * scrollArea = widget.findChild<QScrollArea *>("scrollArea");
        REQUIRE(scrollArea != nullptr);
        REQUIRE(scrollArea->widgetResizable() == true);
    }

    SECTION("Contains feature table widget") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Find the feature table widget
        auto * featureTable = widget.findChild<Feature_Table_Widget *>("feature_table_widget");
        REQUIRE(featureTable != nullptr);
    }

    SECTION("Contains stacked widget") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Find the stacked widget
        auto * stackedWidget = widget.findChild<QStackedWidget *>("stackedWidget");
        REQUIRE(stackedWidget != nullptr);
    }

    SECTION("Contains text overlays section") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Find the MediaText_Widget (should be created as child)
        auto * textWidget = widget.findChild<MediaText_Widget *>();
        REQUIRE(textWidget != nullptr);
    }
}

Q_DECLARE_METATYPE(CanvasCoordinates)

TEST_CASE("MediaPropertiesWidget raises MediaMask_Widget when mask feature selected", "[MediaPropertiesWidget][Mask]") {
    // Ensure a Qt application exists
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    // Create empty DataManager and Media_Window (required for stacked widgets)
    auto state = std::make_shared<MediaWidgetState>();
    auto data_manager = std::make_shared<DataManager>();
    auto media_window = new Media_Window(data_manager);

    MediaPropertiesWidget widget(state, data_manager, media_window);
    widget.setObjectName("media_properties_widget_under_test");

    // Provide a simple timeframe and register it under key "time"
    {
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->removeTime(TimeKey("time"));
        data_manager->setTime(TimeKey("time"), tf);
    }

    // Add a MaskData under key "test_mask"
    auto mask = std::make_shared<MaskData>();
    mask->setImageSize(ImageSize{.width = 640, .height = 480});
    data_manager->setData<MaskData>("test_mask", mask, TimeKey("time"));

    // Allow Feature_Table to observe update and rebuild
    app->processEvents();

    // Find the feature table and its internal QTableWidget
    auto feature_table = widget.findChild<Feature_Table_Widget *>("feature_table_widget");
    REQUIRE(feature_table != nullptr);

    // Ensure it is populated (defensive)
    feature_table->populateTable();
    app->processEvents();

    auto table = feature_table->findChild<QTableWidget *>("available_features_table");
    REQUIRE(table != nullptr);

    // Locate the row for our mask key in the Feature column
    int featureColumnIndex = -1;
    for (int c = 0; c < table->columnCount(); ++c) {
        auto * headerItem = table->horizontalHeaderItem(c);
        if (headerItem && headerItem->text() == QString("Feature")) {
            featureColumnIndex = c;
            break;
        }
    }
    if (featureColumnIndex == -1) featureColumnIndex = 0;

    int mask_row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem * item = table->item(r, featureColumnIndex);
        if (item && item->text() == QString::fromStdString("test_mask")) {
            mask_row = r;
            break;
        }
    }
    REQUIRE(mask_row >= 0);

    // Simulate selecting the row by invoking the private slot that emits featureSelected
    bool const invoked = QMetaObject::invokeMethod(
            feature_table,
            "_highlightFeature",
            Qt::DirectConnection,
            Q_ARG(int, mask_row),
            Q_ARG(int, featureColumnIndex));
    REQUIRE(invoked);

    // Process resulting signal/slot delivery
    app->processEvents();

    // Verify the stacked widget switched to the mask page
    // _featureSelected sets index 3 for DM_DataType::Mask
    auto stack = widget.findChild<QStackedWidget *>("stackedWidget");
    REQUIRE(stack != nullptr);
    REQUIRE(stack->currentIndex() == 3);
}

/**
 * Verify that enabling Brush mode and dragging creates non-empty mask pixels at the current frame.
 * Note: This test requires integration between MediaPropertiesWidget and mask editing UI.
 */
TEST_CASE("MediaPropertiesWidget brush drag creates mask pixels", "[MediaPropertiesWidget][Mask][Brush]") {
    // Ensure a Qt application exists
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    // Register CanvasCoordinates for queued/direct invocation
    qRegisterMetaType<CanvasCoordinates>("CanvasCoordinates");

    auto state = std::make_shared<MediaWidgetState>();
    auto data_manager = std::make_shared<DataManager>();
    auto media_window = new Media_Window(data_manager);

    // Provide timeframe under key "time"
    {
        constexpr int kNumTimes = 200;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->removeTime(TimeKey("time"));
        data_manager->setTime(TimeKey("time"), tf, true);
    }

    // Pre-create target MaskData and register with DataManager
    auto mask = std::make_shared<MaskData>();
    mask->setImageSize(ImageSize{.width = 640, .height = 480});
    data_manager->setData<MaskData>("test_mask", mask, TimeKey("time"));

    MediaPropertiesWidget widget(state, data_manager, media_window);
    constexpr int kWidgetWidth = 900;
    constexpr int kWidgetHeight = 700;
    widget.resize(kWidgetWidth, kWidgetHeight);
    widget.show();
    app->processEvents();

    // Select the mask feature via feature table
    auto feature_table = widget.findChild<Feature_Table_Widget *>("feature_table_widget");
    REQUIRE(feature_table != nullptr);
    feature_table->populateTable();
    app->processEvents();

    auto table = feature_table->findChild<QTableWidget *>("available_features_table");
    REQUIRE(table != nullptr);
    int featureColumnIndex = -1;
    for (int c = 0; c < table->columnCount(); ++c) {
        auto * headerItem = table->horizontalHeaderItem(c);
        if (headerItem && headerItem->text() == QString("Feature")) { featureColumnIndex = c; break; }
    }
    if (featureColumnIndex == -1) featureColumnIndex = 0;

    int mask_row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        auto * item = table->item(r, featureColumnIndex);
        if (item && item->text() == QString::fromStdString("test_mask")) { mask_row = r; break; }
    }
    REQUIRE(mask_row >= 0);

    bool invoked = QMetaObject::invokeMethod(
            feature_table,
            "_highlightFeature",
            Qt::DirectConnection,
            Q_ARG(int, mask_row),
            Q_ARG(int, featureColumnIndex));
    REQUIRE(invoked);
    app->processEvents();

    // Verify mask page raised and obtain the MediaMask_Widget
    auto stack = widget.findChild<QStackedWidget *>("stackedWidget");
    REQUIRE(stack != nullptr);
    REQUIRE(stack->currentIndex() == 3);
    auto mask_widget = qobject_cast<MediaMask_Widget *>(stack->widget(3));
    REQUIRE(mask_widget != nullptr);

    // Ensure the widget is shown so its showEvent connections are made
    mask_widget->show();
    app->processEvents();

    // Switch mouse mode to Brush via combo box
    auto combo = mask_widget->findChild<QComboBox *>("selection_mode_combo");
    REQUIRE(combo != nullptr);
    combo->setCurrentText("Brush");
    app->processEvents();

    // Simulate left-click drag within canvas using the private slots
    // Choose coordinates well within the default canvas (after resize)
    constexpr float kX0 = 150.0f;
    constexpr float kY0 = 120.0f;
    constexpr float kX1 = 165.0f;
    constexpr float kY1 = 135.0f;
    constexpr float kX2 = 180.0f;
    constexpr float kY2 = 150.0f;
    const CanvasCoordinates p0{kX0, kY0};
    const CanvasCoordinates p1{kX1, kY1};
    const CanvasCoordinates p2{kX2, kY2};

    invoked = QMetaObject::invokeMethod(mask_widget, "_clickedInVideo", Qt::DirectConnection,
                                        Q_ARG(CanvasCoordinates, p0));
    REQUIRE(invoked);
    invoked = QMetaObject::invokeMethod(mask_widget, "_mouseMoveInVideo", Qt::DirectConnection,
                                        Q_ARG(CanvasCoordinates, p1));
    REQUIRE(invoked);
    invoked = QMetaObject::invokeMethod(mask_widget, "_mouseMoveInVideo", Qt::DirectConnection,
                                        Q_ARG(CanvasCoordinates, p2));
    REQUIRE(invoked);
    invoked = QMetaObject::invokeMethod(mask_widget, "_mouseReleased", Qt::DirectConnection);
    REQUIRE(invoked);

    app->processEvents();

    // Verify that mask now contains pixels at current time/frame
    auto current_index_and_frame = TimeIndexAndFrame(state->current_position.index, state->current_position.time_frame.get());
    auto const & masks_at_time = mask->getAtTime(current_index_and_frame);
    REQUIRE(!masks_at_time.empty());
    // Check total pixels in primary mask > 0
    REQUIRE(!masks_at_time[0].empty());
}

/**
 * Test drawing on a mask whose image size differs from the default canvas size.
 * The brush drag should still create pixels in mask-space after scaling.
 */
TEST_CASE("MediaPropertiesWidget brush drag creates mask pixels (non-default mask size)", "[MediaPropertiesWidget][Mask][Brush][NonDefaultSize]") {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    qRegisterMetaType<CanvasCoordinates>("CanvasCoordinates");

    auto state = std::make_shared<MediaWidgetState>();
    auto data_manager = std::make_shared<DataManager>();
    auto media_window = new Media_Window(data_manager);
    {
        constexpr int kNumTimes = 200;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->removeTime(TimeKey("time"));
        data_manager->setTime(TimeKey("time"), tf);
    }

    // Use a non-default mask image size (e.g., 320x240)
    auto mask = std::make_shared<MaskData>();
    mask->setImageSize(ImageSize{.width = 320, .height = 240});
    data_manager->setData<MaskData>("mask_small", mask, TimeKey("time"));

    MediaPropertiesWidget widget(state, data_manager, media_window);
    constexpr int kWidgetWidth = 900;
    constexpr int kWidgetHeight = 700;
    widget.resize(kWidgetWidth, kWidgetHeight);
    widget.show();
    app->processEvents();

    // Select the mask feature via feature table
    auto feature_table = widget.findChild<Feature_Table_Widget *>("feature_table_widget");
    REQUIRE(feature_table != nullptr);
    feature_table->populateTable();
    app->processEvents();

    auto table = feature_table->findChild<QTableWidget *>("available_features_table");
    REQUIRE(table != nullptr);
    int featureColumnIndex = -1;
    for (int c = 0; c < table->columnCount(); ++c) {
        auto * headerItem = table->horizontalHeaderItem(c);
        if (headerItem && headerItem->text() == QString("Feature")) { featureColumnIndex = c; break; }
    }
    if (featureColumnIndex == -1) featureColumnIndex = 0;

    int mask_row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        auto * item = table->item(r, featureColumnIndex);
        if (item && item->text() == QString::fromStdString("mask_small")) { mask_row = r; break; }
    }
    REQUIRE(mask_row >= 0);

    bool invoked = QMetaObject::invokeMethod(
            feature_table,
            "_highlightFeature",
            Qt::DirectConnection,
            Q_ARG(int, mask_row),
            Q_ARG(int, featureColumnIndex));
    REQUIRE(invoked);
    app->processEvents();

    auto stack = widget.findChild<QStackedWidget *>("stackedWidget");
    REQUIRE(stack != nullptr);
    REQUIRE(stack->currentIndex() == 3);
    auto mask_widget = qobject_cast<MediaMask_Widget *>(stack->widget(3));
    REQUIRE(mask_widget != nullptr);
    mask_widget->show();
    app->processEvents();

    auto combo = mask_widget->findChild<QComboBox *>("selection_mode_combo");
    REQUIRE(combo != nullptr);
    combo->setCurrentText("Brush");
    app->processEvents();

    // Drag a short stroke
    const CanvasCoordinates p0{150.0f, 120.0f};
    const CanvasCoordinates p1{165.0f, 135.0f};
    const CanvasCoordinates p2{180.0f, 150.0f};
    invoked = QMetaObject::invokeMethod(mask_widget, "_clickedInVideo", Qt::DirectConnection, Q_ARG(CanvasCoordinates, p0));
    REQUIRE(invoked);
    invoked = QMetaObject::invokeMethod(mask_widget, "_mouseMoveInVideo", Qt::DirectConnection, Q_ARG(CanvasCoordinates, p1));
    REQUIRE(invoked);
    invoked = QMetaObject::invokeMethod(mask_widget, "_mouseMoveInVideo", Qt::DirectConnection, Q_ARG(CanvasCoordinates, p2));
    REQUIRE(invoked);
    invoked = QMetaObject::invokeMethod(mask_widget, "_mouseReleased", Qt::DirectConnection);
    REQUIRE(invoked);
    app->processEvents();

    auto const current_index_and_frame = TimeIndexAndFrame(state->current_position.index, state->current_position.time_frame.get());
    auto const & masks_at_time = mask->getAtTime(current_index_and_frame);
    // Expected to be non-empty if scaling from canvas->mask coordinates is correct
    REQUIRE(!masks_at_time.empty());
    REQUIRE(!masks_at_time[0].empty());
}
