#include "GroupManagementWidget.hpp"
#include "GroupManager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QColorDialog>
#include <QDebug>

GroupManagementWidget::GroupManagementWidget(GroupManager* group_manager, QWidget* parent)
    : QWidget(parent),
      m_group_manager(group_manager),
      m_updating_table(false) {
    
    setupUI();
    
    // Connect to GroupManager signals
    connect(m_group_manager, &GroupManager::groupCreated,
            this, &GroupManagementWidget::onGroupCreated);
    connect(m_group_manager, &GroupManager::groupRemoved,
            this, &GroupManagementWidget::onGroupRemoved);
    connect(m_group_manager, &GroupManager::groupModified,
            this, &GroupManagementWidget::onGroupModified);
    
    // Connect table signals
    connect(m_groups_table, &QTableWidget::itemChanged,
            this, &GroupManagementWidget::onItemChanged);
    connect(m_groups_table, &QTableWidget::itemSelectionChanged,
            this, &GroupManagementWidget::onSelectionChanged);
    
    // Connect button signals
    connect(m_add_button, &QPushButton::clicked,
            this, &GroupManagementWidget::createNewGroup);
    connect(m_remove_button, &QPushButton::clicked,
            this, &GroupManagementWidget::removeSelectedGroup);
    
    // Initial table refresh
    refreshTable();
    onSelectionChanged();  // Update button states
}

void GroupManagementWidget::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(5, 5, 5, 5);
    m_layout->setSpacing(5);
    
    // Title
    m_title_label = new QLabel("Data Groups", this);
    m_title_label->setAlignment(Qt::AlignCenter);
    m_title_label->setStyleSheet("font-weight: bold; font-size: 11px; padding: 3px;");
    m_layout->addWidget(m_title_label);
    
    // Groups table
    m_groups_table = new QTableWidget(0, 2, this);  // 2 columns: Name, Color
    m_groups_table->setHorizontalHeaderLabels({"Name", "Color"});
    m_groups_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_groups_table->setAlternatingRowColors(true);
    m_groups_table->setMinimumHeight(120);
    m_groups_table->setMaximumHeight(200);
    
    // Configure table headers
    QHeaderView* header = m_groups_table->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);  // Name column stretches
    header->setSectionResizeMode(1, QHeaderView::Fixed);    // Color column fixed width
    m_groups_table->setColumnWidth(1, 50);  // Color button column width
    
    m_groups_table->verticalHeader()->setVisible(false);
    m_groups_table->setShowGrid(false);
    
    m_layout->addWidget(m_groups_table);
    
    // Buttons
    QHBoxLayout* button_layout = new QHBoxLayout();
    
    m_add_button = new QPushButton("Add Group", this);
    m_add_button->setMaximumHeight(25);
    button_layout->addWidget(m_add_button);
    
    m_remove_button = new QPushButton("Remove", this);
    m_remove_button->setMaximumHeight(25);
    m_remove_button->setEnabled(false);  // Initially disabled
    button_layout->addWidget(m_remove_button);
    
    m_layout->addLayout(button_layout);
}

void GroupManagementWidget::refreshTable() {
    m_updating_table = true;
    
    // Clear the table
    m_groups_table->setRowCount(0);
    
    // Add rows for all groups
    const auto& groups = m_group_manager->getGroups();
    int row = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        addGroupRow(it.key(), row);
        row++;
    }
    
    m_updating_table = false;
}

void GroupManagementWidget::addGroupRow(int group_id, int row) {
    const GroupManager::Group* group = m_group_manager->getGroup(group_id);
    if (!group) {
        return;
    }
    
    m_groups_table->insertRow(row);
    
    // Name column
    QTableWidgetItem* name_item = new QTableWidgetItem(group->name);
    name_item->setData(Qt::UserRole, group_id);  // Store group ID in the item
    m_groups_table->setItem(row, 0, name_item);
    
    // Color column
    QPushButton* color_button = createColorButton(group_id);
    updateColorButton(color_button, group->color);
    m_groups_table->setCellWidget(row, 1, color_button);
}

QPushButton* GroupManagementWidget::createColorButton(int group_id) {
    QPushButton* button = new QPushButton();
    button->setMaximumSize(30, 20);
    button->setMinimumSize(30, 20);
    button->setProperty("group_id", group_id);
    
    connect(button, &QPushButton::clicked,
            this, &GroupManagementWidget::onColorButtonClicked);
    
    return button;
}

void GroupManagementWidget::updateColorButton(QPushButton* button, const QColor& color) {
    QString style = QString("QPushButton { background-color: %1; border: 1px solid #666; }")
                           .arg(color.name());
    button->setStyleSheet(style);
}

void GroupManagementWidget::createNewGroup() {
    QString name = QString("Group %1").arg(m_group_manager->getGroups().size() + 1);
    m_group_manager->createGroup(name);
}

void GroupManagementWidget::removeSelectedGroup() {
    int current_row = m_groups_table->currentRow();
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
        int row = m_groups_table->rowCount();
        addGroupRow(group_id, row);
    }
}

void GroupManagementWidget::onGroupRemoved(int group_id) {
    if (!m_updating_table) {
        int row = findRowForGroupId(group_id);
        if (row >= 0) {
            m_groups_table->removeRow(row);
        }
    }
}

void GroupManagementWidget::onGroupModified(int group_id) {
    if (!m_updating_table) {
        int row = findRowForGroupId(group_id);
        if (row >= 0) {
            const GroupManager::Group* group = m_group_manager->getGroup(group_id);
            if (group) {
                // Update name
                QTableWidgetItem* name_item = m_groups_table->item(row, 0);
                if (name_item) {
                    m_updating_table = true;
                    name_item->setText(group->name);
                    m_updating_table = false;
                }
                
                // Update color button
                QPushButton* color_button = qobject_cast<QPushButton*>(m_groups_table->cellWidget(row, 1));
                if (color_button) {
                    updateColorButton(color_button, group->color);
                }
            }
        }
    }
}

void GroupManagementWidget::onItemChanged(QTableWidgetItem* item) {
    if (m_updating_table || !item || item->column() != 0) {
        return;  // Only handle name column changes
    }
    
    int group_id = item->data(Qt::UserRole).toInt();
    QString new_name = item->text().trimmed();
    
    if (new_name.isEmpty()) {
        // Don't allow empty names, revert to original
        m_updating_table = true;
        const GroupManager::Group* group = m_group_manager->getGroup(group_id);
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
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    
    int group_id = button->property("group_id").toInt();
    const GroupManager::Group* group = m_group_manager->getGroup(group_id);
    if (!group) {
        return;
    }
    
    QColor new_color = QColorDialog::getColor(group->color, this, "Select Group Color");
    if (new_color.isValid() && new_color != group->color) {
        m_group_manager->setGroupColor(group_id, new_color);
    }
}

void GroupManagementWidget::onSelectionChanged() {
    bool has_selection = m_groups_table->currentRow() >= 0;
    m_remove_button->setEnabled(has_selection);
}

int GroupManagementWidget::getGroupIdForRow(int row) const {
    QTableWidgetItem* item = m_groups_table->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

int GroupManagementWidget::findRowForGroupId(int group_id) const {
    for (int row = 0; row < m_groups_table->rowCount(); ++row) {
        if (getGroupIdForRow(row) == group_id) {
            return row;
        }
    }
    return -1;
}
