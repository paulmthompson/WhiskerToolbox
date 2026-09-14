#include <catch2/catch_test_macros.hpp>

#include "Core/MediaWidgetState.hpp"
#include "Media_Widget/UI/Tools/EraserToolOptions_Widget.hpp"

#include <QApplication>
#include <QSlider>
#include <QSpinBox>

#include <array>

namespace {

void ensureQtApplication() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
}

QSlider * findEraserSizeSlider(EraserToolOptions_Widget const & widget) {
    return widget.findChild<QSlider *>("eraser_size_slider");
}

QSpinBox * findEraserSizeSpinbox(EraserToolOptions_Widget const & widget) {
    return widget.findChild<QSpinBox *>("eraser_size_spinbox");
}

}// namespace

TEST_CASE("EraserToolOptions_Widget eraser size controls sync with state", "[EraserToolOptions]") {
    ensureQtApplication();

    MediaWidgetState state;
    EraserToolOptions_Widget widget;
    widget.setState(&state);

    auto * slider = findEraserSizeSlider(widget);
    auto * spinbox = findEraserSizeSpinbox(widget);
    REQUIRE(slider != nullptr);
    REQUIRE(spinbox != nullptr);
    REQUIRE(slider->value() == state.eraserPrefs().radius_px);
    REQUIRE(spinbox->value() == state.eraserPrefs().radius_px);

    slider->setValue(24);
    REQUIRE(state.eraserPrefs().radius_px == 24);
    REQUIRE(spinbox->value() == 24);

    EraserToolPrefs prefs = state.eraserPrefs();
    prefs.radius_px = 8;
    state.setEraserPrefs(prefs);
    REQUIRE(slider->value() == 8);
    REQUIRE(spinbox->value() == 8);
}
