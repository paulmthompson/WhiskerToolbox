#include "GroupManagementWidget.hpp"
#include "GroupManager.hpp"
#include "ui_GroupManagementWidget.h"

#include <QColorDialog>
#include <QDebug>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCheckBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialogButtonBox>

GroupManagementWidget::GroupManagementWidget(GroupManager * group_manager, QWidget * parent)
    : QWidget(parent),
      m_group_manager(group_manager),
      m_ui(new Ui::GroupManagementWidget()),
      m_updating_table(false) {

    m_ui->setupUi(this);
    setupUI();

    // Connect to GroupManager signals
    connect(m_group_manager, &GroupManager::groupCreated,
            this, &GroupManagementWidget::onGroupCreated);
    connect(m_group_manager, &GroupManager::groupRemoved,
            this, &GroupManagementWidget::onGroupRemoved);
    connect(m_group_manager, &GroupManager::groupModified,
            this, &GroupManagementWidget::onGroupModified);

    // Connect table signals
    connect(m_ui->groupsTable, &QTableWidget::itemChanged,
            this, &GroupManagementWidget::onItemChanged);
    connect(m_ui->groupsTable, &QTableWidget::itemSelectionChanged,
            this, &GroupManagementWidget::onSelectionChanged);

    // Connect button signals
    connect(m_ui->addButton, &QPushButton::clicked,
            this, &GroupManagementWidget::createNewGroup);
    connect(m_ui->removeButton, &QPushButton::clicked,
            this, &GroupManagementWidget::removeSelectedGroup);
    
    // Enable context menu
    m_ui->groupsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->groupsTable, &QTableWidget::customContextMenuRequested,
            this, &GroupManagementWidget::showContextMenu);

    // Initial table refresh
    refreshTable();
    onSelectionChanged();// Update button states
}

GroupManagementWidget::~GroupManagementWidget() {
    delete m_ui;
}

void GroupManagementWidget::setupUI() {
    // Configure table headers
    QHeaderView * header = m_ui->groupsTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);// Name column stretches
    header->setSectionResizeMode(1, QHeaderView::Fixed);  // Color column fixed width
    header->setSectionResizeMode(2, QHeaderView::Fixed);  // Visible column fixed width
    header->setSectionResizeMode(3, QHeaderView::Fixed);  // Members column fixed width
    m_ui->groupsTable->setColumnWidth(1, 50);             // Color button column width
    m_ui->groupsTable->setColumnWidth(2, 50);             // Visible checkbox column width
    m_ui->groupsTable->setColumnWidth(3, 60);             // Members column width

    m_ui->groupsTable->verticalHeader()->setVisible(false);
    
    // Enable multi-selection for group operations
    m_ui->groupsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_ui->groupsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void GroupManagementWidget::refreshTable() {
    m_updating_table = true;

    // Clear the table
    m_ui->groupsTable->setRowCount(0);

    // Add rows for all groups
    auto const & groups = m_group_manager->getGroups();
    int row = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        addGroupRow(it.key(), row);
        row++;
    }

    m_updating_table = false;
}

void GroupManagementWidget::addGroupRow(int group_id, int row) {
    auto group = m_group_manager->getGroup(group_id);
    if (!group.has_value()) {
        return;
    }

    m_ui->groupsTable->insertRow(row);

    // Name column
    auto * name_item = new QTableWidgetItem(group.value().name);
    name_item->setData(Qt::UserRole, group_id);// Store group ID in the item
    m_ui->groupsTable->setItem(row, 0, name_item);

    // Color column
    QPushButton * color_button = createColorButton(group_id);
    updateColorButton(color_button, group.value().color);
    m_ui->groupsTable->setCellWidget(row, 1, color_button);

    // Visible column
    QCheckBox * visibility_checkbox = createVisibilityCheckbox(group_id);
    visibility_checkbox->setChecked(group.value().visible);
    m_ui->groupsTable->setCellWidget(row, 2, visibility_checkbox);

    // Members column
    int const member_count = m_group_manager->getGroupMemberCount(group_id);
    auto * members_item = new QTableWidgetItem(QString::number(member_count));
    members_item->setFlags(members_item->flags() & ~Qt::ItemIsEditable);// Make read-only
    members_item->setTextAlignment(Qt::AlignCenter);
    m_ui->groupsTable->setItem(row, 3, members_item);
}

QPushButton * GroupManagementWidget::createColorButton(int group_id) {
    auto * button = new QPushButton();
    button->setMaximumSize(30, 20);
    button->setMinimumSize(30, 20);
    button->setProperty("group_id", group_id);

    connect(button, &QPushButton::clicked,
            this, &GroupManagementWidget::onColorButtonClicked);

    return button;
}

QCheckBox * GroupManagementWidget::createVisibilityCheckbox(int group_id) {
    auto * checkbox = new QCheckBox();
    checkbox->setProperty("group_id", group_id);
    checkbox->setText(""); // No text, just the checkbox

    connect(checkbox, &QCheckBox::toggled,
            this, &GroupManagementWidget::onVisibilityToggled);

    return checkbox;
}

void GroupManagementWidget::updateColorButton(QPushButton * button, QColor const & color) {
    QString const style = QString("QPushButton { background-color: %1; border: 1px solid #666; }")
                            .arg(color.name());
    button->setStyleSheet(style);
}

void GroupManagementWidget::createNewGroup() {
    QString const name = QString("Group %1").arg(m_group_manager->getGroups().size() + 1);
    m_group_manager->createGroup(name);
}

void GroupManagementWidget::removeSelectedGroup() {
    int const current_row = m_ui->groupsTable->currentRow();
    if (current_row < 0) {
        return;
    }

    int const group_id = getGroupIdForRow(current_row);
    if (group_id != -1) {
        m_group_manager->removeGroup(group_id);
    }
}

void GroupManagementWidget::onGroupCreated(int group_id) {
    if (!m_updating_table) {
        int const row = m_ui->groupsTable->rowCount();
        addGroupRow(group_id, row);
    }
}

void GroupManagementWidget::onGroupRemoved(int group_id) {
    if (!m_updating_table) {
        int const row = findRowForGroupId(group_id);
        if (row >= 0) {
            m_ui->groupsTable->removeRow(row);
        }
    }
}

void GroupManagementWidget::onGroupModified(int group_id) {
    if (!m_updating_table) {
        int const row = findRowForGroupId(group_id);
        if (row >= 0) {
            auto group = m_group_manager->getGroup(group_id);
            if (group.has_value()) {
                // Update name
                QTableWidgetItem * name_item = m_ui->groupsTable->item(row, 0);
                if (name_item) {
                    m_updating_table = true;
                    name_item->setText(group.value().name);
                    m_updating_table = false;
                }

                // Update color button
                auto * color_button = qobject_cast<QPushButton *>(m_ui->groupsTable->cellWidget(row, 1));
                if (color_button) {
                    updateColorButton(color_button, group.value().color);
                }

                // Update visibility checkbox
                auto * visibility_checkbox = qobject_cast<QCheckBox *>(m_ui->groupsTable->cellWidget(row, 2));
                if (visibility_checkbox) {
                    visibility_checkbox->setChecked(group.value().visible);
                }

                // Update member count
                QTableWidgetItem * members_item = m_ui->groupsTable->item(row, 3);
                if (members_item) {
                    int const member_count = m_group_manager->getGroupMemberCount(group_id);
                    members_item->setText(QString::number(member_count));
                }
            }
        }
    }
}

void GroupManagementWidget::onPointAssignmentsChanged(std::unordered_set<int> const & affected_groups) {
    if (!m_updating_table) {
        // Update member counts for all affected groups
        for (int const group_id: affected_groups) {
            int const row = findRowForGroupId(group_id);
            if (row >= 0) {
                QTableWidgetItem * members_item = m_ui->groupsTable->item(row, 3);
                if (members_item) {
                    int const member_count = m_group_manager->getGroupMemberCount(group_id);
                    members_item->setText(QString::number(member_count));
                }
            }
        }
    }
}

void GroupManagementWidget::onItemChanged(QTableWidgetItem * item) {
    if (m_updating_table || !item || item->column() != 0) {
        return;// Only handle name column changes
    }

    int const group_id = item->data(Qt::UserRole).toInt();
    QString const new_name = item->text().trimmed();

    if (new_name.isEmpty()) {
        // Don't allow empty names, revert to original
        m_updating_table = true;
        auto group = m_group_manager->getGroup(group_id);
        if (group.has_value()) {
            item->setText(group.value().name);
        }
        m_updating_table = false;
        return;
    }

    // Update the group name
    m_group_manager->setGroupName(group_id, new_name);
}

void GroupManagementWidget::onColorButtonClicked() {
    auto * button = qobject_cast<QPushButton *>(sender());
    if (!button) {
        return;
    }

    int const group_id = button->property("group_id").toInt();
    auto group = m_group_manager->getGroup(group_id);
    if (!group.has_value()) {
        return;
    }

    QColor const new_color = QColorDialog::getColor(group.value().color, this, "Select Group Color");
    if (new_color.isValid() && new_color != group.value().color) {
        m_group_manager->setGroupColor(group_id, new_color);
    }
}

void GroupManagementWidget::onVisibilityToggled(bool visible) {
    auto * checkbox = qobject_cast<QCheckBox *>(sender());
    if (!checkbox) {
        return;
    }

    int const group_id = checkbox->property("group_id").toInt();
    m_group_manager->setGroupVisibility(group_id, visible);
}

void GroupManagementWidget::onSelectionChanged() {
    bool const has_selection = m_ui->groupsTable->currentRow() >= 0;
    m_ui->removeButton->setEnabled(has_selection);
}

int GroupManagementWidget::getGroupIdForRow(int row) const {
    QTableWidgetItem * item = m_ui->groupsTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

int GroupManagementWidget::findRowForGroupId(int group_id) const {
    for (int row = 0; row < m_ui->groupsTable->rowCount(); ++row) {
        if (getGroupIdForRow(row) == group_id) {
            return row;
        }
    }
    return -1;
}

void GroupManagementWidget::contextMenuEvent(QContextMenuEvent * event) {
    showContextMenu(event->globalPos());
}

void GroupManagementWidget::showContextMenu(QPoint const & pos) {
    // Get selected rows
    auto selected_rows = m_ui->groupsTable->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        return;
    }

    // Create context menu
    QMenu context_menu(this);
    
    QAction * merge_action = nullptr;
    QAction * delete_action = nullptr;
    
    // Add merge option if multiple groups are selected
    if (selected_rows.size() >= 2) {
        merge_action = context_menu.addAction("Merge Groups");
        merge_action->setIcon(QIcon::fromTheme("edit-merge"));
        
        context_menu.addSeparator();
    }
    
    // Add delete group and entities action (for single or multiple selection)
    delete_action = context_menu.addAction("Delete Group and All Data");
    delete_action->setIcon(QIcon::fromTheme("edit-delete"));
    
    // Show the menu and handle the action
    QAction * selected_action = context_menu.exec(m_ui->groupsTable->mapToGlobal(pos));
    if (selected_action == merge_action && selected_rows.size() >= 2) {
        showMergeDialog();
    } else if (selected_action == delete_action) {
        deleteSelectedGroupAndEntities();
    }
}

void GroupManagementWidget::deleteSelectedGroupAndEntities() {
    // Get selected rows
    auto selected_rows = m_ui->groupsTable->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        return;
    }

    // Get group information for selected rows
    std::vector<std::pair<int, QString>> selected_groups;
    int total_members = 0;
    
    for (auto const & index : selected_rows) {
        int const group_id = getGroupIdForRow(index.row());
        if (group_id != -1) {
            auto group = m_group_manager->getGroup(group_id);
            if (group.has_value()) {
                selected_groups.emplace_back(group_id, group->name);
                total_members += m_group_manager->getGroupMemberCount(group_id);
            }
        }
    }

    if (selected_groups.empty()) {
        return;
    }

    // Create confirmation message
    QString message;
    if (selected_groups.size() == 1) {
        message = QString("Are you sure you want to delete group '%1' and all %2 entities in it?\n\nThis action cannot be undone.")
                    .arg(selected_groups[0].second)
                    .arg(total_members);
    } else {
        QString group_names;
        for (size_t i = 0; i < selected_groups.size(); ++i) {
            if (i > 0) group_names += ", ";
            group_names += "'" + selected_groups[i].second + "'";
        }
        message = QString("Are you sure you want to delete %1 groups (%2) and all %3 entities in them?\n\nThis action cannot be undone.")
                    .arg(selected_groups.size())
                    .arg(group_names)
                    .arg(total_members);
    }
    
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Delete Groups and Data",
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        // Delete all selected groups
        for (auto const & [group_id, group_name] : selected_groups) {
            m_group_manager->deleteGroupAndEntities(group_id);
        }
    }
}


void GroupManagementWidget::showMergeDialog() {
    // Get selected rows
    auto selected_rows = m_ui->groupsTable->selectionModel()->selectedRows();
    if (selected_rows.size() < 2) {
        return;
    }

    // Get group information for selected rows
    std::vector<std::pair<int, QString>> selected_groups;
    for (auto const & index : selected_rows) {
        int const group_id = getGroupIdForRow(index.row());
        if (group_id != -1) {
            auto group = m_group_manager->getGroup(group_id);
            if (group.has_value()) {
                selected_groups.emplace_back(group_id, group->name);
            }
        }
    }

    if (selected_groups.size() < 2) {
        return;
    }

    // Create merge dialog
    QDialog merge_dialog(this);
    merge_dialog.setWindowTitle("Merge Groups");
    merge_dialog.setModal(true);
    
    QVBoxLayout * layout = new QVBoxLayout(&merge_dialog);
    
    QLabel * instruction_label = new QLabel("Select the target group to merge into:");
    layout->addWidget(instruction_label);
    
    QListWidget * group_list = new QListWidget();
    for (auto const & [group_id, group_name] : selected_groups) {
        QListWidgetItem * item = new QListWidgetItem(group_name);
        item->setData(Qt::UserRole, group_id);
        group_list->addItem(item);
    }
    group_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(group_list);
    
    QHBoxLayout * button_layout = new QHBoxLayout();
    QPushButton * cancel_button = new QPushButton("Cancel");
    QPushButton * merge_button = new QPushButton("Merge");
    merge_button->setDefault(true);
    
    button_layout->addWidget(cancel_button);
    button_layout->addWidget(merge_button);
    layout->addLayout(button_layout);
    
    // Connect buttons
    connect(cancel_button, &QPushButton::clicked, &merge_dialog, &QDialog::reject);
    connect(merge_button, &QPushButton::clicked, &merge_dialog, &QDialog::accept);
    
    // Show dialog
    if (merge_dialog.exec() == QDialog::Accepted) {
        auto selected_items = group_list->selectedItems();
        if (!selected_items.isEmpty()) {
            int const target_group_id = selected_items.first()->data(Qt::UserRole).toInt();
            
            // Collect source group IDs (all except target)
            std::vector<int> source_group_ids;
            for (auto const & [group_id, group_name] : selected_groups) {
                if (group_id != target_group_id) {
                    source_group_ids.push_back(group_id);
                }
            }
            
            // Perform merge
            m_group_manager->mergeGroups(target_group_id, source_group_ids);
        }
    }
}
