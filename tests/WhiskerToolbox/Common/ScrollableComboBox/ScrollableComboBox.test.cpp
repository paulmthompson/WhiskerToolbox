/**
 * @file ScrollableComboBox.test.cpp
 * @brief Unit tests for scroll-limited QComboBox popup configuration.
 */

#include "Common/ScrollableComboBox/ScrollableComboBox.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <catch2/catch_test_macros.hpp>

namespace {

void populateCombo(QComboBox * combo, int item_count) {
    combo->clear();
    for (int i = 0; i < item_count; ++i) {
        combo->addItem(QStringLiteral("item_%1").arg(i));
    }
}

}// namespace

TEST_CASE("configureScrollableComboPopup caps visible rows", "[ScrollableComboBox]") {
    int argc = 0;
    QApplication app(argc, nullptr);

    QComboBox combo;
    populateCombo(&combo, 50);

    wt::widget::configureScrollableComboPopup(&combo, 12);

    REQUIRE(combo.maxVisibleItems() == 12);
    REQUIRE(combo.styleSheet().contains(QStringLiteral("combobox-popup: 0")));
    REQUIRE(combo.view() != nullptr);
    REQUIRE(combo.view()->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded);
}

TEST_CASE("configureScrollableComboPopup does not duplicate style rule", "[ScrollableComboBox]") {
    int argc = 0;
    QApplication app(argc, nullptr);

    QComboBox combo;
    wt::widget::configureScrollableComboPopup(&combo);
    auto const first_stylesheet = combo.styleSheet();

    wt::widget::configureScrollableComboPopup(&combo);

    REQUIRE(combo.styleSheet() == first_stylesheet);
}
