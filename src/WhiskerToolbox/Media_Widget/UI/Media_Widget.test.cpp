
#include "Media_Widget.hpp"

#include "DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <QApplication>
#include <QGraphicsView>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <numeric>
#include <vector>
#include <array>


/**
 * Tests for Media_Widget (view component only).
 * 
 * Note: Tests for feature table selection and mask editing are now in
 * MediaPropertiesWidget.test.cpp since those components have been migrated
 * to MediaPropertiesWidget.
 */

TEST_CASE("Media_Widget construction and basic setup", "[Media_Widget]") {
    // Ensure a Qt application exists
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    // Create EditorRegistry instance required by Media_Widget constructor
    EditorRegistry editor_registry(nullptr);

    SECTION("Constructs with nullptr parent") {
        Media_Widget widget(&editor_registry, nullptr);
        
        // Should have a graphics view
        auto * graphics_view = widget.findChild<QGraphicsView *>("graphicsView");
        REQUIRE(graphics_view != nullptr);
    }

    SECTION("Sets data manager correctly") {
        auto data_manager = std::make_shared<DataManager>();

        Media_Widget widget(&editor_registry, nullptr);
        widget.setDataManager(data_manager);

        // Widget should accept the data manager without crashing
        app->processEvents();
    }

    SECTION("Handles timeframe and mask data") {
        auto data_manager = std::make_shared<DataManager>();

        // Provide a simple timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Add a MaskData
        auto mask = std::make_shared<MaskData>();
        mask->setImageSize(ImageSize{.width = 640, .height = 480});
        data_manager->setData<MaskData>("test_mask", mask, TimeKey("time"));

        Media_Widget widget(&editor_registry, nullptr);
        widget.setDataManager(data_manager);
        
        app->processEvents();

        // Widget should handle data without crashing
        widget.updateMedia();
        app->processEvents();
    }
}
