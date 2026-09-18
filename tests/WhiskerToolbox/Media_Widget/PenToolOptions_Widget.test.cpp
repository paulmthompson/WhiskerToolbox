#include <catch2/catch_test_macros.hpp>

#include "Core/MediaWidgetState.hpp"
#include "Media_Widget/UI/Tools/PenToolOptions_Widget.hpp"

#include <QApplication>
#include <QComboBox>
#include <QLabel>

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

QLabel * findInstructionLabel(PenToolOptions_Widget const & widget) {
    return widget.findChild<QLabel *>("instruction_label");
}

QComboBox * findAppendEndpointCombo(PenToolOptions_Widget const & widget) {
    return widget.findChild<QComboBox *>("append_endpoint_combo");
}

QComboBox * findPenTargetCombo(PenToolOptions_Widget const & widget) {
    return widget.findChild<QComboBox *>("pen_target_combo");
}

}// namespace

TEST_CASE("PenToolOptions_Widget shows Pen tool instructions", "[PenToolOptions]") {
    ensureQtApplication();

    PenToolOptions_Widget widget;
    auto * label = findInstructionLabel(widget);
    REQUIRE(label != nullptr);
    REQUIRE(label->text().contains(QStringLiteral("Click append")));
    REQUIRE(label->text().contains(QStringLiteral("Alt+click")));
    REQUIRE(label->text().contains(QStringLiteral("nearest vertex")));
}

TEST_CASE("PenToolOptions_Widget append endpoint combo syncs with state", "[PenToolOptions]") {
    ensureQtApplication();

    MediaWidgetState state;
    PenToolOptions_Widget widget;
    widget.setState(&state);

    auto * combo = findAppendEndpointCombo(widget);
    REQUIRE(combo != nullptr);
    REQUIRE(combo->count() == 3);
    REQUIRE(combo->currentIndex() == 0);

    combo->setCurrentIndex(1);
    REQUIRE(state.linePrefs().append_endpoint == LineAppendEndpoint::Base);

    LineInteractionPrefs prefs = state.linePrefs();
    prefs.append_endpoint = LineAppendEndpoint::Nearest;
    state.setLinePrefs(prefs);
    REQUIRE(combo->currentIndex() == 2);
}

TEST_CASE("PenToolOptions_Widget pen target combo syncs with state", "[PenToolOptions]") {
    ensureQtApplication();

    MediaWidgetState state;
    state.setFeatureEnabled(QStringLiteral("whisker_a"), QStringLiteral("line"), true);
    state.setFeatureEnabled(QStringLiteral("whisker_b"), QStringLiteral("line"), true);

    PenToolOptions_Widget widget;
    widget.setState(&state);

    auto * combo = findPenTargetCombo(widget);
    REQUIRE(combo != nullptr);
    REQUIRE(combo->count() == 3);
    REQUIRE(combo->currentIndex() == 0);

    combo->setCurrentIndex(1);
    REQUIRE(state.linePrefs().pen_target_mode == PenLineTargetMode::NewLine);
    REQUIRE(state.linePrefs().pen_new_line_key == "whisker_a");

    LineInteractionPrefs prefs = state.linePrefs();
    prefs.pen_target_mode = PenLineTargetMode::NewLine;
    prefs.pen_new_line_key = "whisker_b";
    state.setLinePrefs(prefs);
    REQUIRE(combo->currentIndex() == 2);

    combo->setCurrentIndex(0);
    REQUIRE(state.linePrefs().pen_target_mode == PenLineTargetMode::SelectedLine);
    REQUIRE(state.linePrefs().pen_new_line_key.empty());
}
