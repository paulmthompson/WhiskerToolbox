#ifndef GROUPMANAGEMENTWIDGET_HPP
#define GROUPMANAGEMENTWIDGET_HPP

#include <QWidget>

#include <unordered_set>

class QContextMenuEvent;

class GroupManager;
class QTableWidgetItem;
class QPushButton;

namespace Ui {
class GroupManagementWidget;
}

/**
 * @brief Widget for managing data groups in the toolbox panel
 * 
 * This widget provides a table interface for creating, editing, and managing
 * groups with their names and colors. It integrates with the GroupManager
 * to provide centralized group management across all visualization widgets.
 */
class GroupManagementWidget : public QWidget {
    Q_OBJECT

public:
    explicit GroupManagementWidget(GroupManager * group_manager, QWidget * parent = nullptr);
    ~GroupManagementWidget();

public slots:
    /**
     * @brief Create a new group with a default name
     */
    void createNewGroup();

    /**
     * @brief Remove the currently selected group
     */
    void removeSelectedGroup();

private slots:
    /**
     * @brief Handle when a group is created in the GroupManager
     * @param group_id The ID of the created group
     */
    void onGroupCreated(int group_id);

    /**
     * @brief Handle when a group is removed from the GroupManager
     * @param group_id The ID of the removed group
     */
    void onGroupRemoved(int group_id);

    /**
     * @brief Handle when a group is modified in the GroupManager
     * @param group_id The ID of the modified group
     */
    void onGroupModified(int group_id);

    /**
     * @brief Handle when point assignments change in groups
     * @param affected_groups Set of group IDs that were affected
     */
    void onPointAssignmentsChanged(std::unordered_set<int> const & affected_groups);

    /**
     * @brief Handle when table item is changed (name editing)
     * @param item The changed table item
     */
    void onItemChanged(QTableWidgetItem * item);

    /**
     * @brief Handle when a color button is clicked
     */
    void onColorButtonClicked();

    /**
     * @brief Handle selection changes in the table
     */
    void onSelectionChanged();

    /**
     * @brief Handle context menu events
     */
    void contextMenuEvent(QContextMenuEvent * event) override;

    /**
     * @brief Show context menu at the specified position
     * @param pos Global position where the context menu should appear
     */
    void showContextMenu(QPoint const & pos);

    /**
     * @brief Delete the selected group and all its entities
     */
    void deleteSelectedGroupAndEntities();

private:
    GroupManager * m_group_manager;

    Ui::GroupManagementWidget * m_ui;

    bool m_updating_table;

    /**
     * @brief Initialize the UI components
     */
    void setupUI();

    /**
     * @brief Refresh the entire table from the GroupManager state
     */
    void refreshTable();

    /**
     * @brief Add a row to the table for a specific group
     * @param group_id The group ID
     * @param row The row index to insert at
     */
    void addGroupRow(int group_id, int row);

    /**
     * @brief Create a color button for a group
     * @param group_id The group ID
     * @return The created color button
     */
    QPushButton * createColorButton(int group_id);

    /**
     * @brief Update the color button appearance
     * @param button The button to update
     * @param color The color to display
     */
    void updateColorButton(QPushButton * button, QColor const & color);

    /**
     * @brief Get the group ID for a table row
     * @param row The table row
     * @return The group ID, or -1 if invalid
     */
    [[nodiscard]] int getGroupIdForRow(int row) const;

    /**
     * @brief Find the table row for a group ID
     * @param group_id The group ID
     * @return The row index, or -1 if not found
     */
    [[nodiscard]] int findRowForGroupId(int group_id) const;
};

#endif// GROUPMANAGEMENTWIDGET_HPP
