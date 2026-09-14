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

}// namespace

TEST_CASE("PenToolOptions_Widget shows Pen tool instructions", "[PenToolOptions]") {
    ensureQtApplication();

    PenToolOptions_Widget widget;
    auto * label = findInstructionLabel(widget);
    REQUIRE(label != nullptr);
    REQUIRE(label->text().contains(QStringLiteral("Ctrl+click")));
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
