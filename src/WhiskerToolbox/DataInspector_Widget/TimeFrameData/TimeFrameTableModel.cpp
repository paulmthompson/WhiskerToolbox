#include "TimeFrameTableModel.hpp"

#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

void TimeFrameTableModel::setTimeValues(std::vector<int> const & times,
                                         std::string const & time_key,
                                         EntityRegistry * registry) {
    beginResetModel();
    _all_data.clear();

    for (size_t i = 0; i < times.size(); ++i) {
        TimeFrameTableRow row;
        row.index = static_cast<int64_t>(i);
        row.time_value = times[i];
        row.entity_id = EntityId(0);
        row.group_name = QStringLiteral("No Group");

        // Look up EntityId for this time entry
        if (registry && !time_key.empty()) {
            row.entity_id = registry->ensureId(
                time_key, EntityKind::TimeEntity,
                TimeFrameIndex(static_cast<int64_t>(i)), 0);

            if (_group_manager && row.entity_id != EntityId(0)) {
                int group_id = _group_manager->getEntityGroup(row.entity_id);
                if (group_id != -1) {
                    auto group = _group_manager->getGroup(group_id);
                    if (group.has_value()) {
                        row.group_name = group->name;
                    }
                }
            }
        }

        _all_data.push_back(row);
    }

    _applyGroupFilter();
    endResetModel();
}

void TimeFrameTableModel::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
}

void TimeFrameTableModel::setGroupFilter(int group_id) {
    _filtered_group_id = group_id;
    _applyGroupFilter();
}

void TimeFrameTableModel::clearGroupFilter() {
    setGroupFilter(-1);
}

void TimeFrameTableModel::_applyGroupFilter() {
    beginResetModel();
    _display_data.clear();

    if (_filtered_group_id == -1) {
        _display_data = _all_data;
    } else {
        for (auto const & row : _all_data) {
            if (_group_manager && row.entity_id != EntityId(0)) {
                int entity_group_id = _group_manager->getEntityGroup(row.entity_id);
                if (entity_group_id == _filtered_group_id) {
                    _display_data.push_back(row);
                }
            }
        }
    }
    endResetModel();
}
