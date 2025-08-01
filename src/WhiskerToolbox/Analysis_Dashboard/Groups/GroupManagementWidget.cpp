#include "GroupManagementWidget.hpp"
#include "GroupManager.hpp"
#include "ui_GroupManagementWidget.h"

#include <QColorDialog>
#include <QDebug>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

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
    connect(m_group_manager, &GroupManager::pointAssignmentsChanged,
            this, &GroupManagementWidget::onPointAssignmentsChanged);

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
    header->setSectionResizeMode(2, QHeaderView::Fixed);  // Members column fixed width
    m_ui->groupsTable->setColumnWidth(1, 50);             // Color button column width
    m_ui->groupsTable->setColumnWidth(2, 60);             // Members column width

    m_ui->groupsTable->verticalHeader()->setVisible(false);
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
    GroupManager::Group const * group = m_group_manager->getGroup(group_id);
    if (!group) {
        return;
    }

    m_ui->groupsTable->insertRow(row);

    // Name column
    QTableWidgetItem * name_item = new QTableWidgetItem(group->name);
    name_item->setData(Qt::UserRole, group_id);// Store group ID in the item
    m_ui->groupsTable->setItem(row, 0, name_item);

    // Color column
    QPushButton * color_button = createColorButton(group_id);
    updateColorButton(color_button, group->color);
    m_ui->groupsTable->setCellWidget(row, 1, color_button);

    // Members column
    int member_count = m_group_manager->getGroupMemberCount(group_id);
    QTableWidgetItem * members_item = new QTableWidgetItem(QString::number(member_count));
    members_item->setFlags(members_item->flags() & ~Qt::ItemIsEditable);// Make read-only
    members_item->setTextAlignment(Qt::AlignCenter);
    m_ui->groupsTable->setItem(row, 2, members_item);
}

QPushButton * GroupManagementWidget::createColorButton(int group_id) {
    QPushButton * button = new QPushButton();
    button->setMaximumSize(30, 20);
    button->setMinimumSize(30, 20);
    button->setProperty("group_id", group_id);

    connect(button, &QPushButton::clicked,
            this, &GroupManagementWidget::onColorButtonClicked);

    return button;
}

void GroupManagementWidget::updateColorButton(QPushButton * button, QColor const & color) {
    QString style = QString("QPushButton { background-color: %1; border: 1px solid #666; }")
                            .arg(color.name());
    button->setStyleSheet(style);
}

void GroupManagementWidget::createNewGroup() {
    QString name = QString("Group %1").arg(m_group_manager->getGroups().size() + 1);
    m_group_manager->createGroup(name);
}

void GroupManagementWidget::removeSelectedGroup() {
    int current_row = m_ui->groupsTable->currentRow();
    if (current_row < 0) {
        return;
    }

    int group_id = getGroupIdForRow(current_row);
    if (group_id != -1) {
        m_group_manager->removeGroup(group_id);
    }
}

void GroupManagementWidget::onGroupCreated(int group_id) {
    if (!m_updating_table) {
        int row = m_ui->groupsTable->rowCount();
        addGroupRow(group_id, row);
    }
}

void GroupManagementWidget::onGroupRemoved(int group_id) {
    if (!m_updating_table) {
        int row = findRowForGroupId(group_id);
        if (row >= 0) {
            m_ui->groupsTable->removeRow(row);
        }
    }
}

void GroupManagementWidget::onGroupModified(int group_id) {
    if (!m_updating_table) {
        int row = findRowForGroupId(group_id);
        if (row >= 0) {
            GroupManager::Group const * group = m_group_manager->getGroup(group_id);
            if (group) {
                // Update name
                QTableWidgetItem * name_item = m_ui->groupsTable->item(row, 0);
                if (name_item) {
                    m_updating_table = true;
                    name_item->setText(group->name);
                    m_updating_table = false;
                }

                // Update color button
                QPushButton * color_button = qobject_cast<QPushButton *>(m_ui->groupsTable->cellWidget(row, 1));
                if (color_button) {
                    updateColorButton(color_button, group->color);
                }

                // Update member count
                QTableWidgetItem * members_item = m_ui->groupsTable->item(row, 2);
                if (members_item) {
                    int member_count = m_group_manager->getGroupMemberCount(group_id);
                    members_item->setText(QString::number(member_count));
                }
            }
        }
    }
}

void GroupManagementWidget::onPointAssignmentsChanged(std::unordered_set<int> const & affected_groups) {
    if (!m_updating_table) {
        // Update member counts for all affected groups
        for (int group_id: affected_groups) {
            int row = findRowForGroupId(group_id);
            if (row >= 0) {
                QTableWidgetItem * members_item = m_ui->groupsTable->item(row, 2);
                if (members_item) {
                    int member_count = m_group_manager->getGroupMemberCount(group_id);
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

    int group_id = item->data(Qt::UserRole).toInt();
    QString new_name = item->text().trimmed();

    if (new_name.isEmpty()) {
        // Don't allow empty names, revert to original
        m_updating_table = true;
        GroupManager::Group const * group = m_group_manager->getGroup(group_id);
        if (group) {
            item->setText(group->name);
        }
        m_updating_table = false;
        return;
    }

    // Update the group name
    m_group_manager->setGroupName(group_id, new_name);
}

void GroupManagementWidget::onColorButtonClicked() {
    QPushButton * button = qobject_cast<QPushButton *>(sender());
    if (!button) {
        return;
    }

    int group_id = button->property("group_id").toInt();
    GroupManager::Group const * group = m_group_manager->getGroup(group_id);
    if (!group) {
        return;
    }

    QColor new_color = QColorDialog::getColor(group->color, this, "Select Group Color");
    if (new_color.isValid() && new_color != group->color) {
        m_group_manager->setGroupColor(group_id, new_color);
    }
}

void GroupManagementWidget::onSelectionChanged() {
    bool has_selection = m_ui->groupsTable->currentRow() >= 0;
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
