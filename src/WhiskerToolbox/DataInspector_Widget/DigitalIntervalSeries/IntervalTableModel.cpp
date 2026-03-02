#include "IntervalTableModel.hpp"

#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <iostream>

IntervalTableModel::IntervalTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

void IntervalTableModel::setIntervals(DigitalIntervalSeries const * interval_data) {
    beginResetModel();
    _display_data.clear();
    _all_data.clear();
    _interval_data_source = interval_data;

    if (interval_data) {
        for (auto const & interval_with_id : interval_data->view()) {
            QString group_name = "No Group";
            EntityId eid = interval_with_id.id();

            if (_group_manager && eid != EntityId(0)) {
                int group_id = _group_manager->getEntityGroup(eid);
                if (group_id != -1) {
                    auto group = _group_manager->getGroup(group_id);
                    if (group.has_value()) {
                        group_name = group->name;
                    }
                }
            }

            _all_data.push_back(IntervalTableRow{
                    .interval = interval_with_id.value(),
                    .entity_id = eid,
                    .group_name = group_name});
        }
    }

    _applyGroupFilter();
    endResetModel();
}

void IntervalTableModel::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    if (_interval_data_source) {
        setIntervals(_interval_data_source);
    }
}

void IntervalTableModel::setGroupFilter(int group_id) {
    _filtered_group_id = group_id;
    _applyGroupFilter();
}

void IntervalTableModel::clearGroupFilter() {
    setGroupFilter(-1);
}

int IntervalTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int IntervalTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 3;  // Start, End, Group
}

QVariant IntervalTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= static_cast<int>(_display_data.size())) {
        return QVariant{};
    }

    IntervalTableRow const & row = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QVariant::fromValue(row.interval.start);
        case 1:
            return QVariant::fromValue(row.interval.end);
        case 2:
            return QVariant::fromValue(row.group_name);
        default:
            return QVariant{};
    }
}

QVariant IntervalTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Start");
            case 1:
                return QString("End");
            case 2:
                return QString("Group");
            default:
                return QVariant{};
        }
    }

    return QVariant{};
}

Qt::ItemFlags IntervalTableModel::flags(QModelIndex const & index) const {
    if (!index.isValid()) {
        return Qt::ItemIsEnabled;
    }
    return QAbstractTableModel::flags(index);
}

IntervalTableRow IntervalTableModel::getRowData(int row) const {
    if (row >= 0 && row < static_cast<int>(_display_data.size())) {
        return _display_data[row];
    }
    return IntervalTableRow{Interval{-1, -1}, EntityId(0), "Invalid"};
}

Interval IntervalTableModel::getInterval(int row) const {
    if (row >= 0 && row < static_cast<int>(_display_data.size())) {
        return _display_data[row].interval;
    }
    return Interval{-1, -1};
}

void IntervalTableModel::_applyGroupFilter() {
    beginResetModel();
    _display_data.clear();

    if (_filtered_group_id == -1) {
        _display_data = _all_data;
    } else {
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
