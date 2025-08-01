#include "GroupManager.hpp"
#include <QDebug>

// Define the default color palette using standard colors
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

GroupManager::GroupManager(QObject * parent)
    : QObject(parent),
      m_next_group_id(1) {
}

int GroupManager::createGroup(QString const & name) {
    QColor color = getNextDefaultColor();
    return createGroup(name, color);
}

int GroupManager::createGroup(QString const & name, QColor const & color) {
    int group_id = m_next_group_id++;

    Group group(group_id, name, color);
    m_groups.insert(group_id, group);

    qDebug() << "GroupManager: Created group" << group_id << "with name" << name;

    emit groupCreated(group_id);
    return group_id;
}

bool GroupManager::removeGroup(int group_id) {
    auto it = m_groups.find(group_id);
    if (it == m_groups.end()) {
        return false;
    }

    // Remove all point assignments for this group
    auto const & point_ids = it->point_ids;
    for (int64_t point_id: point_ids) {
        m_point_to_group.remove(point_id);
    }

    m_groups.erase(it);

    qDebug() << "GroupManager: Removed group" << group_id;

    emit groupRemoved(group_id);
    return true;
}

GroupManager::Group const * GroupManager::getGroup(int group_id) const {
    auto it = m_groups.find(group_id);
    return (it != m_groups.end()) ? &it.value() : nullptr;
}

bool GroupManager::setGroupName(int group_id, QString const & name) {
    auto it = m_groups.find(group_id);
    if (it == m_groups.end()) {
        return false;
    }

    it.value().name = name;

    qDebug() << "GroupManager: Updated group" << group_id << "name to" << name;

    emit groupModified(group_id);
    return true;
}

bool GroupManager::setGroupColor(int group_id, QColor const & color) {
    auto it = m_groups.find(group_id);
    if (it == m_groups.end()) {
        return false;
    }

    it.value().color = color;

    qDebug() << "GroupManager: Updated group" << group_id << "color";

    emit groupModified(group_id);
    return true;
}

bool GroupManager::assignPointsToGroup(int group_id, std::unordered_set<int64_t> const & point_ids) {
    auto it = m_groups.find(group_id);
    if (it == m_groups.end()) {
        return false;
    }

    std::unordered_set<int> affected_groups;
    affected_groups.insert(group_id);

    // Remove points from their current groups and assign to new group
    for (int64_t point_id: point_ids) {
        // Check if point was already in another group
        auto current_group_it = m_point_to_group.find(point_id);
        if (current_group_it != m_point_to_group.end()) {
            int old_group_id = current_group_it.value();
            if (old_group_id != group_id) {
                // Remove from old group
                auto old_group_it = m_groups.find(old_group_id);
                if (old_group_it != m_groups.end()) {
                    old_group_it.value().point_ids.erase(point_id);
                    affected_groups.insert(old_group_id);
                }
            }
        }

        // Assign to new group
        it.value().point_ids.insert(point_id);
        m_point_to_group[point_id] = group_id;
    }

    qDebug() << "GroupManager: Assigned" << point_ids.size() << "points to group" << group_id;

    emit pointAssignmentsChanged(affected_groups);
    return true;
}

bool GroupManager::removePointsFromGroup(int group_id, std::unordered_set<int64_t> const & point_ids) {
    auto it = m_groups.find(group_id);
    if (it == m_groups.end()) {
        return false;
    }

    size_t removed_count = 0;
    for (int64_t point_id: point_ids) {
        if (it.value().point_ids.erase(point_id) > 0) {
            m_point_to_group.remove(point_id);
            removed_count++;
        }
    }

    if (removed_count > 0) {
        qDebug() << "GroupManager: Removed" << removed_count << "points from group" << group_id;

        std::unordered_set<int> affected_groups = {group_id};
        emit pointAssignmentsChanged(affected_groups);
    }

    return true;
}

void GroupManager::ungroupPoints(std::unordered_set<int64_t> const & point_ids) {
    std::unordered_set<int> affected_groups;

    for (int64_t point_id: point_ids) {
        auto it = m_point_to_group.find(point_id);
        if (it != m_point_to_group.end()) {
            int group_id = it.value();
            affected_groups.insert(group_id);

            // Remove from group
            auto group_it = m_groups.find(group_id);
            if (group_it != m_groups.end()) {
                group_it.value().point_ids.erase(point_id);
            }

            // Remove from lookup map
            m_point_to_group.erase(it);
        }
    }

    if (!affected_groups.empty()) {
        qDebug() << "GroupManager: Ungrouped" << point_ids.size() << "points from" << affected_groups.size() << "groups";
        emit pointAssignmentsChanged(affected_groups);
    }
}

int GroupManager::getPointGroup(int64_t point_id) const {
    auto it = m_point_to_group.find(point_id);
    return (it != m_point_to_group.end()) ? it.value() : -1;
}

QColor GroupManager::getPointColor(int64_t point_id, QColor const & default_color) const {
    int group_id = getPointGroup(point_id);
    if (group_id == -1) {
        return default_color;// Ungrouped, use default color
    }

    Group const * group = getGroup(group_id);
    return group ? group->color : default_color;
}

std::unordered_set<int64_t> GroupManager::getGroupPoints(int group_id) const {
    auto it = m_groups.find(group_id);
    return (it != m_groups.end()) ? it.value().point_ids : std::unordered_set<int64_t>();
}

int GroupManager::getGroupMemberCount(int group_id) const {
    auto it = m_groups.find(group_id);
    return (it != m_groups.end()) ? static_cast<int>(it.value().point_ids.size()) : 0;
}

void GroupManager::clearAllGroups() {
    qDebug() << "GroupManager: Clearing all groups";

    m_groups.clear();
    m_point_to_group.clear();
    m_next_group_id = 1;

    // Note: We don't emit specific signals here since everything is being cleared
}

QColor GroupManager::getNextDefaultColor() const {
    if (DEFAULT_COLORS.isEmpty()) {
        return QColor(128, 128, 128);// Fallback gray
    }

    // Cycle through the default colors based on current group count
    int color_index = m_groups.size() % DEFAULT_COLORS.size();
    return DEFAULT_COLORS[color_index];
}
