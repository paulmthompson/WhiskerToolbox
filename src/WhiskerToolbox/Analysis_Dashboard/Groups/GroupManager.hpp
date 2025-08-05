#ifndef GROUPMANAGER_HPP
#define GROUPMANAGER_HPP

#include <QColor>
#include <QMap>
#include <QObject>
#include <QString>

#include <cstdint>
#include <unordered_set>

/**
 * @brief Manages groups for data visualization with colors and point assignments
 * 
 * This class provides a centralized system for managing groups of data points
 * across different visualization widgets. Each group has a unique ID, name, and color.
 */
class GroupManager : public QObject {
    Q_OBJECT

public:
    struct Group {
        int id;
        QString name;
        QColor color;
        std::unordered_set<int64_t> point_ids;// Time stamp IDs of points in this group

        Group(int group_id, QString const & group_name, QColor const & group_color)
            : id(group_id),
              name(group_name),
              color(group_color) {}
    };

    explicit GroupManager(QObject * parent = nullptr);
    ~GroupManager() = default;

    /**
     * @brief Create a new group with auto-generated color
     * @param name The name for the new group
     * @return The ID of the created group
     */
    int createGroup(QString const & name);

    /**
     * @brief Create a new group with specified color
     * @param name The name for the new group
     * @param color The color for the new group
     * @return The ID of the created group
     */
    int createGroup(QString const & name, QColor const & color);

    /**
     * @brief Remove a group and unassign all its points
     * @param group_id The ID of the group to remove
     * @return True if the group was removed, false if it didn't exist
     */
    bool removeGroup(int group_id);

    /**
     * @brief Get all groups
     * @return Map of group ID to Group object
     */
    QMap<int, Group> const & getGroups() const { return m_groups; }

    /**
     * @brief Get a specific group by ID
     * @param group_id The group ID
     * @return Pointer to the group, or nullptr if not found
     */
    Group const * getGroup(int group_id) const;

    /**
     * @brief Update group name
     * @param group_id The group ID
     * @param name The new name
     * @return True if successful, false if group doesn't exist
     */
    bool setGroupName(int group_id, QString const & name);

    /**
     * @brief Update group color
     * @param group_id The group ID
     * @param color The new color
     * @return True if successful, false if group doesn't exist
     */
    bool setGroupColor(int group_id, QColor const & color);

    /**
     * @brief Assign points to a group
     * @param group_id The target group ID
     * @param point_ids Set of time stamp IDs to assign
     * @return True if successful, false if group doesn't exist
     */
    bool assignPointsToGroup(int group_id, std::unordered_set<int64_t> const & point_ids);

    /**
     * @brief Remove points from a group
     * @param group_id The group ID
     * @param point_ids Set of time stamp IDs to remove
     * @return True if successful, false if group doesn't exist
     */
    bool removePointsFromGroup(int group_id, std::unordered_set<int64_t> const & point_ids);

    /**
     * @brief Remove points from all groups (ungroup them)
     * @param point_ids Set of time stamp IDs to ungroup
     */
    void ungroupPoints(std::unordered_set<int64_t> const & point_ids);

    /**
     * @brief Get which group a point belongs to
     * @param point_id The time stamp ID of the point
     * @return Group ID if found, -1 if ungrouped
     */
    int getPointGroup(int64_t point_id) const;

    /**
     * @brief Get the color for a point based on its group assignment
     * @param point_id The time stamp ID of the point
     * @param default_color The color to use if the point is ungrouped
     * @return The color to use for rendering this point
     */
    QColor getPointColor(int64_t point_id, QColor const & default_color) const;

    /**
     * @brief Get all point IDs assigned to a specific group
     * @param group_id The group ID
     * @return Set of time stamp IDs in the group, empty if group doesn't exist
     */
    std::unordered_set<int64_t> getGroupPoints(int group_id) const;

    /**
     * @brief Get the number of points assigned to a specific group
     * @param group_id The group ID
     * @return Number of points in the group, 0 if group doesn't exist
     */
    int getGroupMemberCount(int group_id) const;

    /**
     * @brief Clear all groups and assignments
     */
    void clearAllGroups();

signals:
    /**
     * @brief Emitted when a new group is created
     * @param group_id The ID of the created group
     */
    void groupCreated(int group_id);

    /**
     * @brief Emitted when a group is removed
     * @param group_id The ID of the removed group
     */
    void groupRemoved(int group_id);

    /**
     * @brief Emitted when group properties change (name or color)
     * @param group_id The ID of the modified group
     */
    void groupModified(int group_id);

    /**
     * @brief Emitted when point assignments change
     * @param affected_groups Set of group IDs that were affected
     */
    void pointAssignmentsChanged(std::unordered_set<int> const & affected_groups);

private:
    QMap<int, Group> m_groups;
    QMap<int64_t, int> m_point_to_group;// Fast lookup for point -> group mapping
    int m_next_group_id;

    static QVector<QColor> const DEFAULT_COLORS;

    /**
     * @brief Get the next color from the default palette
     * @return A color from the predefined palette
     */
    QColor getNextDefaultColor() const;
};

#endif// GROUPMANAGER_HPP
