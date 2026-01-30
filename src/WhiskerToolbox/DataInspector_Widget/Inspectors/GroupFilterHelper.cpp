#include "GroupFilterHelper.hpp"

#include "GroupManagementWidget/GroupManager.hpp"

#include <QComboBox>

void populateGroupFilterCombo(QComboBox * combo_box, GroupManager * group_manager) {
    if (!combo_box) {
        return;
    }

    // Block signals temporarily to avoid triggering filter changes during population
    combo_box->blockSignals(true);
    combo_box->clear();
    combo_box->addItem("All Groups");

    if (!group_manager) {
        combo_box->setCurrentIndex(0);  // Ensure "All Groups" is selected
        combo_box->blockSignals(false);
        return;
    }

    auto groups = group_manager->getGroups();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        combo_box->addItem(it.value().name);
    }
    
    // Ensure "All Groups" is selected by default if no valid selection
    if (combo_box->currentIndex() < 0) {
        combo_box->setCurrentIndex(0);
    }
    combo_box->blockSignals(false);
}

void restoreGroupFilterSelection(QComboBox * combo_box, int previous_index, QString const & previous_text) {
    if (!combo_box) {
        return;
    }

    // Try to restore by index first
    if (previous_index >= 0 && previous_index < combo_box->count()) {
        combo_box->setCurrentIndex(previous_index);
        return;
    }

    // If index doesn't work, try to find by text
    if (!previous_text.isEmpty()) {
        int found_index = combo_box->findText(previous_text);
        if (found_index >= 0) {
            combo_box->setCurrentIndex(found_index);
            return;
        }
    }

    // Fall back to "All Groups"
    combo_box->setCurrentIndex(0);
}
