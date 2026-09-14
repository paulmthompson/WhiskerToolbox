#include <catch2/catch_test_macros.hpp>

#include "Media_Widget/UI/Tools/PenToolOptions_Widget.hpp"

#include <QApplication>
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
