#ifndef GROUPMANAGER_HPP
#define GROUPMANAGER_HPP

#include <QColor>
#include <QMap>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <unordered_set>

#include "DataManager/Entity/EntityTypes.hpp"

class EntityGroupManager;

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

        Group() : id(0), name(""), color(QColor()) {}
        Group(int group_id, QString const & group_name, QColor const & group_color)
            : id(group_id),
              name(group_name),
              color(group_color) {}
    };

    explicit GroupManager(EntityGroupManager* entity_group_manager, QObject * parent = nullptr);
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
    QMap<int, Group> getGroups() const;

    /**
     * @brief Get a specific group by ID
     * @param group_id The group ID
     * @return Optional Group object if found
     */
    std::optional<Group> getGroup(int group_id) const;

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

    // ===== EntityId-based API =====
    /**
     * @brief Assign entities (EntityId) to a group
     */
    bool assignEntitiesToGroup(int group_id, std::unordered_set<EntityId> const & entity_ids);

    /**
     * @brief Remove entities (EntityId) from a group
     */
    bool removeEntitiesFromGroup(int group_id, std::unordered_set<EntityId> const & entity_ids);

    /**
     * @brief Ungroup a set of entities
     */
    void ungroupEntities(std::unordered_set<EntityId> const & entity_ids);

    /**
     * @brief Get which group an entity belongs to
     */
    int getEntityGroup(EntityId id) const;

    /**
     * @brief Get the color for an entity based on its group assignment
     */
    QColor getEntityColor(EntityId id, QColor const & default_color) const;

    /**
     * @brief Get the number of entities assigned to a specific group
     * @param group_id The group ID
     * @return Number of entities in the group, 0 if group doesn't exist
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
    //void pointAssignmentsChanged(std::unordered_set<int> const & affected_groups);

private:
    EntityGroupManager* m_entity_group_manager;
    QMap<int, QColor> m_group_colors; // Maps EntityGroupManager GroupId to QColor
    int m_next_group_id;

    static QVector<QColor> const DEFAULT_COLORS;

    /**
     * @brief Get the next color from the default palette
     * @return A color from the predefined palette
     */
    QColor getNextDefaultColor() const;
};

#endif// GROUPMANAGER_HPP
