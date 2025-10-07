#include "LineTableModel.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <iostream>

LineTableModel::LineTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

void LineTableModel::setLines(LineData const * lineData) {
    beginResetModel();
    _display_data.clear();
    _all_data.clear();
    _line_data_source = lineData;
    if (lineData) {
        for (auto const & timeLineEntriesPair: lineData->GetAllLineEntriesAsRange()) {
            auto frame = timeLineEntriesPair.time.getValue();
            int lineIndex = 0;
            for (auto const & entry: timeLineEntriesPair.entries) {
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
                
                LineTableRow row = {
                    .frame = frame, 
                    .lineIndex = lineIndex, 
                    .length = static_cast<int>(entry.line.size()),
                    .entity_id = entry.entity_id,
                    .group_name = group_name
                };
                _all_data.push_back(row);
                lineIndex++;
            }
        }
    }
    _applyGroupFilter();
    endResetModel();
}

int LineTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int LineTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 4;// Frame, Line Index, Length, Group
}

QVariant LineTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= _display_data.size()) {
        return QVariant{};
    }

    LineTableRow const & rowData = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(rowData.frame);
        case 1:
            return QVariant::fromValue(rowData.lineIndex);
        case 2:
            return QVariant::fromValue(rowData.length);
        case 3:
            return QVariant::fromValue(rowData.group_name);
        default:
            return QVariant{};
    }
}

QVariant LineTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Frame");
            case 1:
                return QString("Line Index");
            case 2:
                return QString("Length (points)");
            case 3:
                return QString("Group");
            default:
                return QVariant{};
        }
    }
    return QVariant{};// No vertical header
}

LineTableRow LineTableModel::getRowData(int row) const {
    if (row >= 0 && row < _display_data.size()) {
        return _display_data[row];
    }
    // Return a default/invalid LineTableRow or throw an exception
    // For simplicity, returning a default-constructed one here, but error handling might be better.
    return LineTableRow{-1, -1, -1, 0, "Invalid"};
}

void LineTableModel::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    // Refresh data to update group names
    if (_line_data_source) {
        setLines(_line_data_source);
    }
}

void LineTableModel::setGroupFilter(int group_id) {
    _filtered_group_id = group_id;
    _applyGroupFilter();
}

void LineTableModel::clearGroupFilter() {
    setGroupFilter(-1);
}

void LineTableModel::_applyGroupFilter() {
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