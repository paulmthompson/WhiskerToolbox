/**
 * @file ScrollableComboBox.cpp
 * @brief Implementation of scroll-limited QComboBox popup configuration.
 */

#include "ScrollableComboBox.hpp"

#include <cassert>

#include <QAbstractItemView>
#include <QComboBox>
#include <QString>

namespace wt::widget {

namespace {

constexpr char const * kComboPopupRule = "QComboBox { combobox-popup: 0; }";

/**
 * @brief Append the combobox-popup style rule without duplicating it.
 * @param combo Non-owning combo box to update.
 *
 * @pre combo must not be null.
 */
void appendComboPopupStyleSheet(QComboBox * combo) {
    assert(combo != nullptr);
    QString const existing = combo->styleSheet();
    if (existing.contains(QStringLiteral("combobox-popup"))) {
        return;
    }
    if (existing.isEmpty()) {
        combo->setStyleSheet(QString::fromUtf8(kComboPopupRule));
    } else {
        combo->setStyleSheet(existing + QLatin1Char('\n') + QString::fromUtf8(kComboPopupRule));
    }
}

}// namespace

void configureScrollableComboPopup(QComboBox * combo, int max_visible_items) {
    assert(combo != nullptr && "configureScrollableComboPopup: combo must not be null");
    if (max_visible_items < 0) {
        max_visible_items = kDefaultComboMaxVisibleItems;
    }

    appendComboPopupStyleSheet(combo);
    combo->setMaxVisibleItems(max_visible_items);
    if (auto * view = combo->view()) {
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
}

}// namespace wt::widget
