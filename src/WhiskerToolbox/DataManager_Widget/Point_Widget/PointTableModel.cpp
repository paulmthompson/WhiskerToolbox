#include "PointTableModel.hpp"

#include "DataManager/Points/Point_Data.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <iostream>

PointTableModel::PointTableModel(QObject * parent)
    : QAbstractTableModel(parent), _display_data{}, _all_data{} {}

void PointTableModel::setPoints(PointData const * pointData) {
    beginResetModel();
    _display_data.clear();
    _all_data.clear();
    _point_data_source = pointData;
    if (pointData) {
        for (auto const & timePointEntriesPair: pointData->GetAllPointEntriesAsRange()) {
            auto frame = timePointEntriesPair.time.getValue();
            int pointIndex = 0;
            for (auto const & entry: timePointEntriesPair.entries) {
                QString group_name = "No Group";
                if (_group_manager) {
                    int group_id = _group_manager->getEntityGroup(entry.entity_id);
                    if (group_id != -1) {
                        auto group = _group_manager->getGroup(group_id);
                        if (group.has_value()) {
                            group_name = group->name;
                        }
                    }
                }
                
                PointTableRow row = {
                    .frame = frame, 
                    .pointIndex = pointIndex, 
                    .x = entry.data.x,
                    .y = entry.data.y,
                    .entity_id = entry.entity_id,
                    .group_name = group_name
                };
                _all_data.push_back(row);
                pointIndex++;
            }
        }
    }
    _applyGroupFilter();
    endResetModel();
}

int PointTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int PointTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 5; // Frame, Point Index, X, Y, Group
}

QVariant PointTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= _display_data.size() || index.row() < 0) {
        return QVariant{};
    }

    PointTableRow const & rowData = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(rowData.frame);
        case 1:
            return QVariant::fromValue(rowData.pointIndex);
        case 2:
            return QVariant::fromValue(rowData.x);
        case 3:
            return QVariant::fromValue(rowData.y);
        case 4:
            return QVariant::fromValue(rowData.group_name);
        default:
            return QVariant{};
    }
}

QVariant PointTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Frame");
            case 1:
                return QString("Point Index");
            case 2:
                return QString("X");
            case 3:
                return QString("Y");
            case 4:
                return QString("Group");
            default:
                return QVariant{};
        }
    }
    return QVariant{}; // No vertical header
}

PointTableRow PointTableModel::getRowData(int row) const {
    if (row >= 0 && row < _display_data.size()) {
        return _display_data[row];
    }
    // Return a default/invalid PointTableRow or throw an exception
    // For simplicity, returning a default-constructed one here, but error handling might be better.
    return PointTableRow{-1, -1, 0.0f, 0.0f, EntityId(0), "Invalid"};
}

void PointTableModel::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    // Refresh data to update group names
    if (_point_data_source) {
        setPoints(_point_data_source);
    }
}

void PointTableModel::setGroupFilter(int group_id) {
    _filtered_group_id = group_id;
    _applyGroupFilter();
}

void PointTableModel::clearGroupFilter() {
    setGroupFilter(-1);
}

void PointTableModel::_applyGroupFilter() {
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
