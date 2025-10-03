#ifndef GROUPMANAGER_HPP
#define GROUPMANAGER_HPP

#include "Entity/EntityTypes.hpp"

#include <QColor>
#include <QMap>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <memory>

class EntityGroupManager;
class DataManager;
class PointData;
class LineData;

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
        bool visible;

        Group()
            : id(0),
              name(""),
              color(QColor()),
              visible(true) {}
        Group(int group_id, QString const & group_name, QColor const & group_color)
            : id(group_id),
              name(group_name),
              color(group_color),
              visible(true) {}
        Group(int group_id, QString const & group_name, QColor const & group_color, bool group_visible)
            : id(group_id),
              name(group_name),
              color(group_color),
              visible(group_visible) {}
    };

    explicit GroupManager(EntityGroupManager * entity_group_manager, std::shared_ptr<DataManager> data_manager, QObject * parent = nullptr);
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
     * @brief Delete a group and all entities in it from their respective data objects
     * @param group_id The ID of the group to delete
     * @return True if the group and entities were deleted, false if group didn't exist
     */
    bool deleteGroupAndEntities(int group_id);

    /**
     * @brief Get all groups
     * @return Map of group ID to Group object
     */
    [[nodiscard]] QMap<int, Group> getGroups() const;

    /**
     * @brief Get a specific group by ID
     * @param group_id The group ID
     * @return Optional Group object if found
     */
    [[nodiscard]] std::optional<Group> getGroup(int group_id) const;

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
     * @brief Update group visibility
     * @param group_id The group ID
     * @param visible The new visibility state
     * @return True if successful, false if group doesn't exist
     */
    bool setGroupVisibility(int group_id, bool visible);

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
    [[nodiscard]] int getEntityGroup(EntityId id) const;

    /**
     * @brief Get the color for an entity based on its group assignment
     */
    [[nodiscard]] QColor getEntityColor(EntityId id, QColor const & default_color) const;

    /**
     * @brief Check if an entity's group is visible
     * @param id The entity ID
     * @return True if the entity's group is visible, false if not in a group or group is hidden
     */
    [[nodiscard]] bool isEntityGroupVisible(EntityId id) const;

    /**
     * @brief Get the number of entities assigned to a specific group
     * @param group_id The group ID
     * @return Number of entities in the group, 0 if group doesn't exist
     */
    [[nodiscard]] int getGroupMemberCount(int group_id) const;

    /**
     * @brief Clear all groups and assignments
     */
    void clearAllGroups();

    // ===== Common Group Operations for Context Menus =====
    /**
     * @brief Create a new group and assign the given entities to it
     * @param entity_ids Set of EntityIds to assign to the new group
     * @return The ID of the created group, or -1 if failed
     */
    int createGroupWithEntities(std::unordered_set<EntityId> const & entity_ids);

    /**
     * @brief Get a list of groups that can be used in context menus
     * @return Vector of pairs containing {group_id, group_name}
     */
    std::vector<std::pair<int, QString>> getGroupsForContextMenu() const;

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

private:
    EntityGroupManager * m_entity_group_manager;
    std::shared_ptr<DataManager> m_data_manager;
    QMap<int, QColor> m_group_colors;// Maps EntityGroupManager GroupId to QColor
    QMap<int, bool> m_group_visibility;// Maps EntityGroupManager GroupId to visibility state
    int m_next_group_id;

    static QVector<QColor> const DEFAULT_COLORS;

    /**
     * @brief Get the next color from the default palette
     * @return A color from the predefined palette
     */
    [[nodiscard]] QColor getNextDefaultColor() const;

    /**
     * @brief Remove an entity from all data objects
     * @param entity_id The entity to remove
     */
    void removeEntityFromDataObjects(EntityId entity_id);

    /**
     * @brief Remove an entity from PointData
     * @param point_data The PointData object
     * @param entity_id The entity to remove
     */
    void removeEntityFromPointData(PointData * point_data, EntityId entity_id);

    /**
     * @brief Remove an entity from LineData
     * @param line_data The LineData object
     * @param entity_id The entity to remove
     */
    void removeEntityFromLineData(LineData * line_data, EntityId entity_id);

};

#endif// GROUPMANAGER_HPP
