#include <catch2/catch_test_macros.hpp>

#include "Core/LineEditOperations.hpp"
#include "Core/MediaWidgetState.hpp"
#include "Media_Widget/UI/Tools/SmoothToolOptions_Widget.hpp"

#include <QApplication>
#include <QComboBox>
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

QSlider * findSmoothSizeSlider(SmoothToolOptions_Widget const & widget) {
    return widget.findChild<QSlider *>("smooth_size_slider");
}

QSpinBox * findSmoothSizeSpinbox(SmoothToolOptions_Widget const & widget) {
    return widget.findChild<QSpinBox *>("smooth_size_spinbox");
}

QComboBox * findAlgorithmCombo(SmoothToolOptions_Widget const & widget) {
    return widget.findChild<QComboBox *>("algorithm_combo");
}

QSlider * findStrengthSlider(SmoothToolOptions_Widget const & widget) {
    return widget.findChild<QSlider *>("strength_slider");
}

QSpinBox * findPolynomialOrderSpinbox(SmoothToolOptions_Widget const & widget) {
    return widget.findChild<QSpinBox *>("polynomial_order_spinbox");
}

}// namespace

TEST_CASE("SmoothToolOptions_Widget brush size controls sync with state", "[SmoothToolOptions]") {
    ensureQtApplication();

    MediaWidgetState state;
    SmoothToolOptions_Widget widget;
    widget.setState(&state);

    auto * slider = findSmoothSizeSlider(widget);
    auto * spinbox = findSmoothSizeSpinbox(widget);
    REQUIRE(slider != nullptr);
    REQUIRE(spinbox != nullptr);
    REQUIRE(slider->value() == state.smoothPrefs().radius_px);
    REQUIRE(spinbox->value() == state.smoothPrefs().radius_px);

    slider->setValue(24);
    REQUIRE(state.smoothPrefs().radius_px == 24);
    REQUIRE(spinbox->value() == 24);

    SmoothToolPrefs prefs = state.smoothPrefs();
    prefs.radius_px = 8;
    state.setSmoothPrefs(prefs);
    REQUIRE(slider->value() == 8);
    REQUIRE(spinbox->value() == 8);
}

TEST_CASE("SmoothToolOptions_Widget algorithm and strength controls sync with state", "[SmoothToolOptions]") {
    ensureQtApplication();

    MediaWidgetState state;
    SmoothToolOptions_Widget widget;
    widget.setState(&state);

    auto * algorithm_combo = findAlgorithmCombo(widget);
    auto * strength_slider = findStrengthSlider(widget);
    auto * order_spinbox = findPolynomialOrderSpinbox(widget);
    REQUIRE(algorithm_combo != nullptr);
    REQUIRE(strength_slider != nullptr);
    REQUIRE(order_spinbox != nullptr);

    algorithm_combo->setCurrentIndex(1);
    REQUIRE(state.smoothPrefs().algorithm == LineSmoothAlgorithm::PolynomialFit);
    REQUIRE(order_spinbox->isEnabled());
    REQUIRE_FALSE(strength_slider->isEnabled());

    algorithm_combo->setCurrentIndex(0);
    REQUIRE(state.smoothPrefs().algorithm == LineSmoothAlgorithm::MovingAverage);
    REQUIRE(strength_slider->isEnabled());
    REQUIRE_FALSE(order_spinbox->isEnabled());

    strength_slider->setValue(3);
    REQUIRE(state.smoothPrefs().strength == 3);

    algorithm_combo->setCurrentIndex(1);
    order_spinbox->setValue(5);
    REQUIRE(state.smoothPrefs().polynomial_order == 5);
}
