#include "GroupManager.hpp"

#include "Entity/EntityGroupManager.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"

#include <QDebug>

QVector<QColor> const GroupManager::DEFAULT_COLORS = {
        QColor(31, 119, 180), // Blue
        QColor(255, 127, 14), // Orange
        QColor(44, 160, 44),  // Green
        QColor(214, 39, 40),  // Red
        QColor(148, 103, 189),// Purple
        QColor(140, 86, 75),  // Brown
        QColor(227, 119, 194),// Pink
        QColor(127, 127, 127),// Gray
        QColor(188, 189, 34), // Olive
        QColor(23, 190, 207)  // Cyan
};

GroupManager::GroupManager(EntityGroupManager * entity_group_manager, std::shared_ptr<DataManager> data_manager, QObject * parent)
    : QObject(parent),
      m_entity_group_manager(entity_group_manager),
      m_data_manager(std::move(data_manager)),
      m_next_group_id(1) {
    // Assert that we have valid managers
    Q_ASSERT(m_entity_group_manager != nullptr);
    Q_ASSERT(m_data_manager != nullptr);
}

int GroupManager::createGroup(QString const & name) {
    QColor const color = getNextDefaultColor();
    return createGroup(name, color);
}

int GroupManager::createGroup(QString const & name, QColor const & color) {

    GroupId const entity_group_id = m_entity_group_manager->createGroup(name.toStdString());

    auto const group_id = static_cast<int>(entity_group_id);
    m_group_colors[group_id] = color;
    m_group_visibility[group_id] = true; // Groups are visible by default

    qDebug() << "GroupManager: Created group" << group_id << "with name" << name;

    emit groupCreated(group_id);
    return group_id;
}

bool GroupManager::removeGroup(int group_id) {
    auto entity_group_id = static_cast<GroupId>(group_id);

    if (!m_entity_group_manager->deleteGroup(entity_group_id)) {
        return false;
    }

    m_group_colors.remove(group_id);
    m_group_visibility.remove(group_id);

    qDebug() << "GroupManager: Removed group" << group_id;

    emit groupRemoved(group_id);
    return true;
}

std::optional<GroupManager::Group> GroupManager::getGroup(int group_id) const {
    auto entity_group_id = static_cast<GroupId>(group_id);

    auto descriptor = m_entity_group_manager->getGroupDescriptor(entity_group_id);
    if (!descriptor.has_value()) {
        return std::nullopt;
    }

    Group group(group_id,
                QString::fromStdString(descriptor->name),
                m_group_colors.value(group_id, QColor(128, 128, 128)),
                m_group_visibility.value(group_id, true));

    return group;
}

bool GroupManager::setGroupName(int group_id, QString const & name) {
    auto entity_group_id = static_cast<GroupId>(group_id);

    // Get current descriptor to preserve description
    auto descriptor = m_entity_group_manager->getGroupDescriptor(entity_group_id);
    if (!descriptor.has_value()) {
        return false;
    }

    // Avoid redundant updates/signals if the name is unchanged
    if (QString::fromStdString(descriptor->name) == name) {
        return true;
    }

    if (!m_entity_group_manager->updateGroup(entity_group_id, name.toStdString(), descriptor->description)) {
        return false;
    }

    qDebug() << "GroupManager: Updated group" << group_id << "name to" << name;

    emit groupModified(group_id);
    return true;
}

bool GroupManager::setGroupColor(int group_id, QColor const & color) {
    auto entity_group_id = static_cast<GroupId>(group_id);

    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }

    m_group_colors[group_id] = color;

    qDebug() << "GroupManager: Updated group" << group_id << "color";

    emit groupModified(group_id);
    return true;
}

bool GroupManager::setGroupVisibility(int group_id, bool visible) {
    auto entity_group_id = static_cast<GroupId>(group_id);

    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }

    m_group_visibility[group_id] = visible;

    qDebug() << "GroupManager: Updated group" << group_id << "visibility to" << visible;

    emit groupModified(group_id);
    return true;
}

QMap<int, GroupManager::Group> GroupManager::getGroups() const {
    QMap<int, Group> result;

    auto group_ids = m_entity_group_manager->getAllGroupIds();
    for (GroupId const entity_group_id: group_ids) {
        auto descriptor = m_entity_group_manager->getGroupDescriptor(entity_group_id);
        if (descriptor.has_value()) {
            auto group_id = static_cast<int>(entity_group_id);
            Group const group(group_id,
                              QString::fromStdString(descriptor->name),
                              m_group_colors.value(group_id, QColor(128, 128, 128)),
                              m_group_visibility.value(group_id, true));
            result[group_id] = group;
        }
    }

    return result;
}

// ===== EntityId-based API =====
bool GroupManager::assignEntitiesToGroup(int group_id, std::unordered_set<EntityId> const & entity_ids) {
    auto entity_group_id = static_cast<GroupId>(group_id);

    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }

    std::vector<EntityId> const entity_vector(entity_ids.begin(), entity_ids.end());

    std::size_t const added_count = m_entity_group_manager->addEntitiesToGroup(entity_group_id, entity_vector);

    qDebug() << "GroupManager: Assigned" << added_count << "entities to group" << group_id;
    if (added_count > 0) {
        emit groupModified(group_id);
    }

    return added_count > 0;
}

bool GroupManager::removeEntitiesFromGroup(int group_id, std::unordered_set<EntityId> const & entity_ids) {
    auto entity_group_id = static_cast<GroupId>(group_id);

    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }

    std::vector<EntityId> const entity_vector(entity_ids.begin(), entity_ids.end());

    std::size_t const removed_count = m_entity_group_manager->removeEntitiesFromGroup(entity_group_id, entity_vector);

    if (removed_count > 0) {
        qDebug() << "GroupManager: Removed" << removed_count << "entities from group" << group_id;
        emit groupModified(group_id);
    }

    return removed_count > 0;
}

void GroupManager::ungroupEntities(std::unordered_set<EntityId> const & entity_ids) {
    std::unordered_set<int> affected_groups;

    // For each entity, find which groups it belongs to and remove it
    for (EntityId const entity_id: entity_ids) {
        auto group_ids = m_entity_group_manager->getGroupsContainingEntity(entity_id);
        for (GroupId const entity_group_id: group_ids) {
            auto const group_id = static_cast<int>(entity_group_id);
            affected_groups.insert(group_id);

            std::vector<EntityId> const single_entity = {entity_id};
            m_entity_group_manager->removeEntitiesFromGroup(entity_group_id, single_entity);
        }
    }

    if (!affected_groups.empty()) {
        qDebug() << "GroupManager: Ungrouped" << entity_ids.size() << "entities from" << affected_groups.size() << "groups";
        for (int const gid: affected_groups) {
            emit groupModified(gid);
        }
    }
}

int GroupManager::getEntityGroup(EntityId id) const {
    auto group_ids = m_entity_group_manager->getGroupsContainingEntity(id);

    // Return the first group (assuming entities belong to at most one group for now)
    if (!group_ids.empty()) {
        return static_cast<int>(group_ids[0]);
    }

    return -1;// Not in any group
}

QColor GroupManager::getEntityColor(EntityId id, QColor const & default_color) const {
    int const group_id = getEntityGroup(id);
    if (group_id == -1) {
        return default_color;
    }

    return m_group_colors.value(group_id, default_color);
}

bool GroupManager::isEntityGroupVisible(EntityId id) const {
    int const group_id = getEntityGroup(id);
    if (group_id == -1) {
        return true; // Entities not in a group are always visible
    }

    return m_group_visibility.value(group_id, true);
}

int GroupManager::getGroupMemberCount(int group_id) const {
    auto entity_group_id = static_cast<GroupId>(group_id);
    return static_cast<int>(m_entity_group_manager->getGroupSize(entity_group_id));
}

void GroupManager::clearAllGroups() {
    qDebug() << "GroupManager: Clearing all groups";

    // Clear EntityGroupManager
    m_entity_group_manager->clear();

    // Clear our color and visibility mappings
    m_group_colors.clear();
    m_group_visibility.clear();

    m_next_group_id = 1;

    // Note: We don't emit specific signals here since everything is being cleared
}

QColor GroupManager::getNextDefaultColor() const {
    if (DEFAULT_COLORS.isEmpty()) {
        return {128, 128, 128};// Fallback gray
    }

    // Cycle through the default colors based on current group count
    auto group_ids = m_entity_group_manager->getAllGroupIds();
    auto color_index = static_cast<int>(group_ids.size()) % DEFAULT_COLORS.size();
    return DEFAULT_COLORS[color_index];
}

// ===== Common Group Operations for Context Menus =====

int GroupManager::createGroupWithEntities(std::unordered_set<EntityId> const & entity_ids) {
    if (entity_ids.empty()) {
        return -1;
    }

    QString group_name = QString("Group %1").arg(m_entity_group_manager->getAllGroupIds().size() + 1);
    int group_id = createGroup(group_name);
    
    if (group_id != -1) {
        assignEntitiesToGroup(group_id, entity_ids);
    }
    
    return group_id;
}

std::vector<std::pair<int, QString>> GroupManager::getGroupsForContextMenu() const {
    std::vector<std::pair<int, QString>> result;
    
    auto groups = getGroups();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        result.emplace_back(it.key(), it.value().name);
    }
    
    return result;
}

bool GroupManager::mergeGroups(int target_group_id, std::vector<int> const & source_group_ids) {
    // Validate target group exists
    auto target_entity_group_id = static_cast<GroupId>(target_group_id);
    if (!m_entity_group_manager->hasGroup(target_entity_group_id)) {
        qDebug() << "GroupManager: Target group" << target_group_id << "does not exist";
        return false;
    }

    // Validate source groups exist and are different from target
    for (int source_group_id : source_group_ids) {
        if (source_group_id == target_group_id) {
            qDebug() << "GroupManager: Cannot merge group into itself:" << source_group_id;
            return false;
        }
        
        auto source_entity_group_id = static_cast<GroupId>(source_group_id);
        if (!m_entity_group_manager->hasGroup(source_entity_group_id)) {
            qDebug() << "GroupManager: Source group" << source_group_id << "does not exist";
            return false;
        }
    }

    // Collect all entities from source groups
    std::unordered_set<EntityId> entities_to_merge;
    for (int source_group_id : source_group_ids) {
        auto source_entity_group_id = static_cast<GroupId>(source_group_id);
        auto entities_in_group = m_entity_group_manager->getEntitiesInGroup(source_entity_group_id);
        
        for (EntityId entity_id : entities_in_group) {
            entities_to_merge.insert(entity_id);
        }
    }

    // Move all entities to target group
    if (!entities_to_merge.empty()) {
        std::vector<EntityId> entities_vector(entities_to_merge.begin(), entities_to_merge.end());
        m_entity_group_manager->addEntitiesToGroup(target_entity_group_id, entities_vector);
    }

    // Remove source groups
    for (int source_group_id : source_group_ids) {
        auto source_entity_group_id = static_cast<GroupId>(source_group_id);
        
        // Remove all entities from source group first
        auto entities_in_group = m_entity_group_manager->getEntitiesInGroup(source_entity_group_id);
        if (!entities_in_group.empty()) {
            std::vector<EntityId> entities_vector(entities_in_group.begin(), entities_in_group.end());
            m_entity_group_manager->removeEntitiesFromGroup(source_entity_group_id, entities_vector);
        }
        
        // Delete the source group
        m_entity_group_manager->deleteGroup(source_entity_group_id);
        
        // Clean up our mappings
        m_group_colors.remove(source_group_id);
        m_group_visibility.remove(source_group_id);
        
        qDebug() << "GroupManager: Removed source group" << source_group_id;
        emit groupRemoved(source_group_id);
    }

    qDebug() << "GroupManager: Merged" << source_group_ids.size() << "groups into group" << target_group_id;
    emit groupModified(target_group_id);
    
    return true;
}

bool GroupManager::deleteGroupAndEntities(int group_id) {
    auto entity_group_id = static_cast<GroupId>(group_id);

    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }

    // Get all entities in the group
    auto entities = m_entity_group_manager->getEntitiesInGroup(entity_group_id);
    if (entities.empty()) {
        // No entities to delete, just remove the group
        return removeGroup(group_id);
    }

    qDebug() << "GroupManager: Deleting group" << group_id << "with" << entities.size() << "entities";

    // Remove entities from their respective data objects
    for (EntityId const entity_id : entities) {
        removeEntityFromDataObjects(entity_id);
    }

    // Remove the group (this will also remove all entity-group associations)
    bool const group_removed = removeGroup(group_id);

    if (group_removed) {
        qDebug() << "GroupManager: Successfully deleted group" << group_id << "and all its entities";
    }

    return group_removed;
}

void GroupManager::removeEntityFromDataObjects(EntityId entity_id) {
    // Get all data keys from the DataManager
    auto const data_keys = m_data_manager->getAllKeys();
    
    for (std::string const & key : data_keys) {
        auto data_variant = m_data_manager->getDataVariant(key);
        if (!data_variant.has_value()) {
            continue;
        }

        // Handle different data types
        std::visit([this, &key, entity_id](auto & data_ptr) {
            if constexpr (std::is_same_v<std::decay_t<decltype(data_ptr)>, std::shared_ptr<PointData>>) {
                if (data_ptr) {
                    removeEntityFromPointData(data_ptr.get(), entity_id);
                }
            } else if constexpr (std::is_same_v<std::decay_t<decltype(data_ptr)>, std::shared_ptr<LineData>>) {
                if (data_ptr) {
                    removeEntityFromLineData(data_ptr.get(), entity_id);
                }
            }
            // Note: DigitalEventSeries and DigitalIntervalSeries don't have entity lookup methods
            // so we skip them for now
        }, data_variant.value());
    }
}

void GroupManager::removeEntityFromPointData(PointData * point_data, EntityId entity_id) {
    if (!point_data) return;

    // Find the time and index for this entity
    auto time_and_index = point_data->getTimeAndIndexByEntityId(entity_id);
    if (!time_and_index.has_value()) {
        return;
    }

    auto const [time, index] = time_and_index.value();
    
    // Remove the point at the specific time and index
    point_data->clearAtTime(time, static_cast<size_t>(index), true);
}

void GroupManager::removeEntityFromLineData(LineData * line_data, EntityId entity_id) {
    if (!line_data) return;

    // Find the time and index for this entity
    auto time_and_index = line_data->getTimeAndIndexByEntityId(entity_id);
    if (!time_and_index.has_value()) {
        return;
    }

    auto const [time, index] = time_and_index.value();
    
    // Remove the line at the specific time and index
    line_data->clearAtTime(time, index, true);
}

