/**
 * @file LinePlotTimeAxisRange.test.cpp
 * @brief Tests for LinePlot time-axis range control synchronization
 *
 * Verifies the silent-update contract between RelativeTimeAxisState,
 * LinePlotState, and LinePlotWidget range spinboxes (including wheel input).
 */

#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/LinePlotWidget/Core/LinePlotState.hpp"
#include "Plots/LinePlotWidget/UI/LinePlotWidget.hpp"

#include "DataManager/DataManager.hpp"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QSignalSpy>
#include <QWheelEvent>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

namespace {

void wheelSpinBox(QDoubleSpinBox * box, int steps) {
    box->resize(120, 30);
    box->show();
    QApplication::processEvents();

    QPoint const pos(box->width() / 2, box->height() / 2);
    QWheelEvent event(pos,
                      box->mapToGlobal(pos),
                      QPoint(),
                      QPoint(0, steps * 120),
                      Qt::NoButton,
                      Qt::NoModifier,
                      Qt::ScrollPhase::NoScrollPhase,
                      false);
    QApplication::sendEvent(box, &event);
    QApplication::processEvents();
}

}// namespace

// =============================================================================
// State-level silent-update contract
// =============================================================================

TEST_CASE("LinePlotState setRangeSilent does not emit stateChanged", "[LinePlotTimeAxisRange]") {
    int argc = 0;
    QApplication const app(argc, nullptr);

    auto state = std::make_shared<LinePlotState>();
    QSignalSpy const state_changed_spy(state.get(), &LinePlotState::stateChanged);

    state->relativeTimeAxisState()->setRangeSilent(-250.0, 250.0);
    QApplication::processEvents();

    REQUIRE(state_changed_spy.count() == 0);
}

TEST_CASE("LinePlotState setRangeSilent does not change alignment window_size",
          "[LinePlotTimeAxisRange]") {
    int argc = 0;
    QApplication const app(argc, nullptr);

    auto state = std::make_shared<LinePlotState>();
    double const initial_window = state->getWindowSize();
    REQUIRE(initial_window == Catch::Approx(1000.0));

    state->relativeTimeAxisState()->setRangeSilent(-250.0, 250.0);
    QApplication::processEvents();

    REQUIRE(state->getWindowSize() == Catch::Approx(initial_window));
}

TEST_CASE("LinePlotState user rangeChanged does not change window_size", "[LinePlotTimeAxisRange]") {
    int argc = 0;
    QApplication const app(argc, nullptr);

    auto state = std::make_shared<LinePlotState>();
    double const initial_window = state->getWindowSize();

    state->relativeTimeAxisState()->setRange(-400.0, 400.0);
    QApplication::processEvents();

    REQUIRE(state->getWindowSize() == Catch::Approx(initial_window));
}

// =============================================================================
// Widget-level zoom <-> spinbox contract
// =============================================================================

TEST_CASE("LinePlotWidget zoom updates time range spinboxes to visible range",
          "[LinePlotTimeAxisRange]") {
    int argc = 0;
    QApplication const app(argc, nullptr);

    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<LinePlotState>();

    LinePlotWidget plot_widget(data_manager);
    plot_widget.resize(800, 600);
    plot_widget.show();
    plot_widget.setState(state);
    QApplication::processEvents();

    auto * range_controls = plot_widget.getRangeControls();
    REQUIRE(range_controls != nullptr);

    state->setXZoom(2.0);
    QApplication::processEvents();

    REQUIRE_THAT(range_controls->minRangeSpinBox()->value(),
                 Catch::Matchers::WithinAbs(-250.0, 0.5));
    REQUIRE_THAT(range_controls->maxRangeSpinBox()->value(),
                 Catch::Matchers::WithinAbs(250.0, 0.5));
}

TEST_CASE("LinePlotWidget spinbox edit updates x zoom to match visible span",
          "[LinePlotTimeAxisRange]") {
    int argc = 0;
    QApplication const app(argc, nullptr);

    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<LinePlotState>();

    LinePlotWidget plot_widget(data_manager);
    plot_widget.resize(800, 600);
    plot_widget.show();
    plot_widget.setState(state);
    QApplication::processEvents();

    auto * range_controls = plot_widget.getRangeControls();
    REQUIRE(range_controls != nullptr);

    range_controls->minRangeSpinBox()->setValue(-200.0);
    range_controls->maxRangeSpinBox()->setValue(200.0);
    QApplication::processEvents();

    REQUIRE_THAT(state->viewState().x_zoom, Catch::Matchers::WithinAbs(2.5, 0.05));
}

TEST_CASE("LinePlotWidget wheel on min spinbox is reversible", "[LinePlotTimeAxisRange]") {
    int argc = 0;
    QApplication const app(argc, nullptr);

    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<LinePlotState>();

    LinePlotWidget plot_widget(data_manager);
    plot_widget.resize(800, 600);
    plot_widget.show();
    plot_widget.setState(state);
    QApplication::processEvents();

    auto * min_spinbox = plot_widget.getRangeControls()->minRangeSpinBox();
    REQUIRE(min_spinbox != nullptr);

    double const initial_min = min_spinbox->value();
    wheelSpinBox(min_spinbox, 3);
    wheelSpinBox(min_spinbox, -3);

    REQUIRE_THAT(min_spinbox->value(), Catch::Matchers::WithinAbs(initial_min, 0.5));
}

TEST_CASE("LinePlotWidget manual spinbox value survives after processEvents",
          "[LinePlotTimeAxisRange]") {
    int argc = 0;
    QApplication const app(argc, nullptr);

    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<LinePlotState>();

    LinePlotWidget plot_widget(data_manager);
    plot_widget.resize(800, 600);
    plot_widget.show();
    plot_widget.setState(state);
    QApplication::processEvents();

    auto * min_spinbox = plot_widget.getRangeControls()->minRangeSpinBox();
    REQUIRE(min_spinbox != nullptr);

    min_spinbox->setValue(-300.0);
    QApplication::processEvents();

    REQUIRE_THAT(min_spinbox->value(), Catch::Matchers::WithinAbs(-300.0, 0.5));
}
