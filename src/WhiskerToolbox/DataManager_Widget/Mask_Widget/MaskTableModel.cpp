#include "MaskTableModel.hpp"

#include "DataManager/Masks/Mask_Data.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <iostream>// For debugging

MaskTableModel::MaskTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

void MaskTableModel::setMasks(MaskData const * maskData) {
    beginResetModel();
    _display_data.clear();
    _all_data.clear();
    _mask_data_source = maskData;
    if (maskData) {
        for (auto const & timeMaskPair: maskData->getAllAsRange()) {
            auto frame = timeMaskPair.time.getValue();
            int totalPoints = 0;
            EntityId const entity_id = 0; // Default to 0 for masks without entity ID
            QString group_name = "No Group";
            
            // For masks, we need to get the entity ID from the mask data
            // Since masks don't have individual entity IDs like lines, we'll use a default approach
            // This might need to be adjusted based on how masks are associated with entities
            
            if (_group_manager && entity_id != 0) {
                int const group_id = _group_manager->getEntityGroup(entity_id);
                if (group_id != -1) {
                    auto group = _group_manager->getGroup(group_id);
                    if (group.has_value()) {
                        group_name = group->name;
                    }
                }
            }
            
            for (auto const & mask: timeMaskPair.masks) {
                totalPoints += static_cast<int>(mask.size());
            }
            
            MaskTableRow const row = {
                .frame = frame,
                .totalPointsInFrame = totalPoints,
                .entity_id = entity_id,
                .group_name = group_name
            };
            _all_data.push_back(row);
        }
    }
    _applyGroupFilter();
    endResetModel();
}

int MaskTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int MaskTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 3;// Frame, Total Points, Group
}

QVariant MaskTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= _display_data.size() || index.row() < 0) {
        return QVariant{};
    }

    MaskTableRow const & rowData = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(rowData.frame);
        case 1:
            return QVariant::fromValue(rowData.totalPointsInFrame);
        case 2:
            return QVariant::fromValue(rowData.group_name);
        default:
            return QVariant{};
    }
}

QVariant MaskTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Frame");
            case 1:
                return QString("Total Points in Frame");
            case 2:
                return QString("Group");
            default:
                return QVariant{};
        }
    }
    return QVariant{};// No vertical header
}

int MaskTableModel::getFrameForRow(int row) const {
    if (row >= 0 && static_cast<size_t>(row) < _display_data.size()) {
        return static_cast<int>(_display_data[row].frame);
    }
    return -1;// Invalid row
}

MaskTableRow MaskTableModel::getRowData(int row) const {
    if (row >= 0 && row < _display_data.size()) {
        return _display_data[row];
    }
    // Return a default/invalid MaskTableRow or throw an exception
    // For simplicity, returning a default-constructed one here, but error handling might be better.
    return MaskTableRow{-1, -1, 0, "Invalid"};
}

void MaskTableModel::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    // Refresh data to update group names
    if (_mask_data_source) {
        setMasks(_mask_data_source);
    }
}

void MaskTableModel::setGroupFilter(int group_id) {
    _filtered_group_id = group_id;
    _applyGroupFilter();
}

void MaskTableModel::clearGroupFilter() {
    setGroupFilter(-1);
}

void MaskTableModel::_applyGroupFilter() {
    beginResetModel();
    _display_data.clear();
    
    if (_filtered_group_id == -1) {
        // Show all groups
        _display_data = _all_data;
    } else {
        // Filter by specific group
        for (auto const & row : _all_data) {
            if (_group_manager) {
                int entity_group_id = _group_manager->getEntityGroup(row.entity_id);
                if (entity_group_id == _filtered_group_id) {
                    _display_data.push_back(row);
                }
            }
        }
    }
    endResetModel();
}
