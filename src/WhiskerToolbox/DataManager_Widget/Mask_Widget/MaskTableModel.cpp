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
        for (auto const & [time, entries]: maskData->getAllEntries()) {
            auto frame = time.getValue();
            int maskIndex = 0;
            for (auto const & entry: entries) {
                QString group_name = "No Group";
                if (_group_manager) {
                    int const group_id = _group_manager->getEntityGroup(entry.entity_id);
                    if (group_id != -1) {
                        auto group = _group_manager->getGroup(group_id);
                        if (group.has_value()) {
                            group_name = group->name;
                        }
                    }
                }

                MaskTableRow const row = {
                    .frame = frame,
                    .maskIndex = maskIndex,
                    .totalPointsInFrame = static_cast<int>(entry.data.size()),
                    .entity_id = entry.entity_id,
                    .group_name = group_name
                };
                _all_data.push_back(row);
                maskIndex++;
            }
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
    return 4;// Frame, Mask Index, Total Points, Group
}

QVariant MaskTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() < 0 || static_cast<size_t>(index.row()) >= _display_data.size()) {
        return QVariant{};
    }

    MaskTableRow const & rowData = _display_data.at(static_cast<size_t>(index.row()));

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(rowData.frame);
        case 1:
            return QVariant::fromValue(rowData.maskIndex);
        case 2:
            return QVariant::fromValue(rowData.totalPointsInFrame);
        case 3:
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
                return QString("Mask Index");
            case 2:
                return QString("Total Points");
            case 3:
                return QString("Group");
            default:
                return QVariant{};
        }
    }
    return QVariant{};// No vertical header
}

int MaskTableModel::getFrameForRow(int row) const {
    if (row >= 0 && static_cast<size_t>(row) < _display_data.size()) {
        return static_cast<int>(_display_data[static_cast<size_t>(row)].frame);
    }
    return -1;// Invalid row
}

MaskTableRow MaskTableModel::getRowData(int row) const {
    if (row >= 0 && static_cast<size_t>(row) < _display_data.size()) {
        return _display_data[static_cast<size_t>(row)];
    }
    // Return a default/invalid MaskTableRow or throw an exception
    // For simplicity, returning a default-constructed one here, but error handling might be better.
    return MaskTableRow{-1, -1, -1, EntityId(0), "Invalid"};
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
