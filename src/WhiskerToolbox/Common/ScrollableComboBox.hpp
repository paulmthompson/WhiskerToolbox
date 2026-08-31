/**
 * @file ScrollableComboBox.hpp
 * @brief Utilities for constraining QComboBox popup height with scrollbars.
 */

#ifndef SCROLLABLE_COMBO_BOX_HPP
#define SCROLLABLE_COMBO_BOX_HPP

class QComboBox;

namespace wt::widget {

/// Default number of visible rows before the popup shows a scrollbar.
constexpr int kDefaultComboMaxVisibleItems = 15;

/**
 * @brief Force a QListView-based popup and cap visible rows.
 *
 * On Gtk+/Linux, non-editable QComboBox popups use a native menu that ignores
 * maxVisibleItems. Setting combobox-popup: 0 switches to an internal list view
 * that honors the row cap and scrolls for the remaining items.
 *
 * @param combo Non-owning combo box to configure.
 * @param max_visible_items Maximum rows shown before scrolling (default 15).
 *
 * @pre combo must not be null.
 * @post combo uses a scroll-limited list popup when opened.
 */
void configureScrollableComboPopup(QComboBox * combo,
                                   int max_visible_items = kDefaultComboMaxVisibleItems);

}// namespace wt::widget

#endif// SCROLLABLE_COMBO_BOX_HPP
