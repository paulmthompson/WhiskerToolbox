#include "GroupManager.hpp"
#include "DataManager/Entity/EntityGroupManager.hpp"

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

GroupManager::GroupManager(EntityGroupManager* entity_group_manager, QObject * parent)
    : QObject(parent),
      m_entity_group_manager(entity_group_manager),
      m_next_group_id(1) {
    // Assert that we have a valid EntityGroupManager
    Q_ASSERT(m_entity_group_manager != nullptr);
}

int GroupManager::createGroup(QString const & name) {
    QColor color = getNextDefaultColor();
    return createGroup(name, color);
}

int GroupManager::createGroup(QString const & name, QColor const & color) {
    // Create group in EntityGroupManager
    GroupId entity_group_id = m_entity_group_manager->createGroup(name.toStdString());
    
    // Store color mapping
    int group_id = static_cast<int>(entity_group_id);
    m_group_colors[group_id] = color;

    qDebug() << "GroupManager: Created group" << group_id << "with name" << name;

    emit groupCreated(group_id);
    return group_id;
}

bool GroupManager::removeGroup(int group_id) {
    GroupId entity_group_id = static_cast<GroupId>(group_id);
    
    // Remove from EntityGroupManager
    if (!m_entity_group_manager->deleteGroup(entity_group_id)) {
        return false;
    }
    
    // Remove color mapping
    m_group_colors.remove(group_id);

    qDebug() << "GroupManager: Removed group" << group_id;

    emit groupRemoved(group_id);
    return true;
}

std::optional<GroupManager::Group> GroupManager::getGroup(int group_id) const {
    GroupId entity_group_id = static_cast<GroupId>(group_id);
    
    auto descriptor = m_entity_group_manager->getGroupDescriptor(entity_group_id);
    if (!descriptor.has_value()) {
        return std::nullopt;
    }
    
    Group group(group_id, 
                QString::fromStdString(descriptor->name),
                m_group_colors.value(group_id, QColor(128, 128, 128)));
    
    return group;
}

bool GroupManager::setGroupName(int group_id, QString const & name) {
    GroupId entity_group_id = static_cast<GroupId>(group_id);
    
    // Get current descriptor to preserve description
    auto descriptor = m_entity_group_manager->getGroupDescriptor(entity_group_id);
    if (!descriptor.has_value()) {
        return false;
    }
    
    // Update group in EntityGroupManager
    if (!m_entity_group_manager->updateGroup(entity_group_id, name.toStdString(), descriptor->description)) {
        return false;
    }

    qDebug() << "GroupManager: Updated group" << group_id << "name to" << name;

    emit groupModified(group_id);
    return true;
}

bool GroupManager::setGroupColor(int group_id, QColor const & color) {
    GroupId entity_group_id = static_cast<GroupId>(group_id);
    
    // Check if group exists in EntityGroupManager
    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }
    
    // Update color mapping
    m_group_colors[group_id] = color;

    qDebug() << "GroupManager: Updated group" << group_id << "color";

    emit groupModified(group_id);
    return true;
}

QMap<int, GroupManager::Group> GroupManager::getGroups() const {
    QMap<int, Group> result;
    
    auto group_ids = m_entity_group_manager->getAllGroupIds();
    for (GroupId entity_group_id : group_ids) {
        auto descriptor = m_entity_group_manager->getGroupDescriptor(entity_group_id);
        if (descriptor.has_value()) {
            int group_id = static_cast<int>(entity_group_id);
            Group group(group_id,
                       QString::fromStdString(descriptor->name),
                       m_group_colors.value(group_id, QColor(128, 128, 128)));
            result[group_id] = group;
        }
    }
    
    return result;
}

// ===== EntityId-based API =====
bool GroupManager::assignEntitiesToGroup(int group_id, std::unordered_set<EntityId> const & entity_ids) {
    GroupId entity_group_id = static_cast<GroupId>(group_id);
    
    // Check if group exists
    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }
    
    // Convert to vector for EntityGroupManager API
    std::vector<EntityId> entity_vector(entity_ids.begin(), entity_ids.end());
    
    // Add entities to group
    std::size_t added_count = m_entity_group_manager->addEntitiesToGroup(entity_group_id, entity_vector);

    qDebug() << "GroupManager: Assigned" << added_count << "entities to group" << group_id;
    if (added_count > 0) {
        emit groupModified(group_id);
    }
    
    // For now, emit the old signal for compatibility
    //std::unordered_set<int> affected_groups = {group_id};
    //emit pointAssignmentsChanged(affected_groups);
    
    return added_count > 0;
}

bool GroupManager::removeEntitiesFromGroup(int group_id, std::unordered_set<EntityId> const & entity_ids) {
    GroupId entity_group_id = static_cast<GroupId>(group_id);
    
    // Check if group exists
    if (!m_entity_group_manager->hasGroup(entity_group_id)) {
        return false;
    }
    
    // Convert to vector for EntityGroupManager API
    std::vector<EntityId> entity_vector(entity_ids.begin(), entity_ids.end());
    
    // Remove entities from group
    std::size_t removed_count = m_entity_group_manager->removeEntitiesFromGroup(entity_group_id, entity_vector);

    if (removed_count > 0) {
        qDebug() << "GroupManager: Removed" << removed_count << "entities from group" << group_id;
        emit groupModified(group_id);
    }

    return removed_count > 0;
}

void GroupManager::ungroupEntities(std::unordered_set<EntityId> const & entity_ids) {
    std::unordered_set<int> affected_groups;

    // For each entity, find which groups it belongs to and remove it
    for (EntityId entity_id : entity_ids) {
        auto group_ids = m_entity_group_manager->getGroupsContainingEntity(entity_id);
        for (GroupId entity_group_id : group_ids) {
            int group_id = static_cast<int>(entity_group_id);
            affected_groups.insert(group_id);
            
            // Remove entity from this group
            std::vector<EntityId> single_entity = {entity_id};
            m_entity_group_manager->removeEntitiesFromGroup(entity_group_id, single_entity);
        }
    }

    if (!affected_groups.empty()) {
        qDebug() << "GroupManager: Ungrouped" << entity_ids.size() << "entities from" << affected_groups.size() << "groups";
        for (int gid : affected_groups) {
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
    
    return -1; // Not in any group
}

QColor GroupManager::getEntityColor(EntityId id, QColor const & default_color) const {
    int group_id = getEntityGroup(id);
    if (group_id == -1) {
        return default_color;
    }
    
    return m_group_colors.value(group_id, default_color);
}

int GroupManager::getGroupMemberCount(int group_id) const {
    GroupId entity_group_id = static_cast<GroupId>(group_id);
    return static_cast<int>(m_entity_group_manager->getGroupSize(entity_group_id));
}

void GroupManager::clearAllGroups() {
    qDebug() << "GroupManager: Clearing all groups";

    // Clear EntityGroupManager
    m_entity_group_manager->clear();
    
    // Clear our color mappings
    m_group_colors.clear();
    
    m_next_group_id = 1;

    // Note: We don't emit specific signals here since everything is being cleared
}

QColor GroupManager::getNextDefaultColor() const {
    if (DEFAULT_COLORS.isEmpty()) {
        return QColor(128, 128, 128);// Fallback gray
    }

    // Cycle through the default colors based on current group count
    auto group_ids = m_entity_group_manager->getAllGroupIds();
    int color_index = static_cast<int>(group_ids.size()) % DEFAULT_COLORS.size();
    return DEFAULT_COLORS[color_index];
}
