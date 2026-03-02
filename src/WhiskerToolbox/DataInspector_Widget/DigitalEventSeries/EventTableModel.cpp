/**
 * @file EventTableModel.cpp
 * @brief Implementation of EventTableModel with group support
 */

#include "EventTableModel.hpp"

#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <iostream>

EventTableModel::EventTableModel(QObject * parent)
    : QAbstractTableModel(parent) {}

void EventTableModel::setEvents(DigitalEventSeries const * event_data) {
    beginResetModel();
    _display_data.clear();
    _all_data.clear();
    _event_data_source = event_data;

    if (event_data) {
        for (auto const & event_with_id: event_data->view()) {
            QString group_name = "No Group";
            EntityId eid = event_with_id.id();

            if (_group_manager && eid != EntityId(0)) {
                int group_id = _group_manager->getEntityGroup(eid);
                if (group_id != -1) {
                    auto group = _group_manager->getGroup(group_id);
                    if (group.has_value()) {
                        group_name = group->name;
                    }
                }
            }

            _all_data.push_back(EventTableRow{
                    .time = event_with_id.time(),
                    .entity_id = eid,
                    .group_name = group_name});
        }
    }

    _applyGroupFilter();
    endResetModel();
}

void EventTableModel::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    if (_event_data_source) {
        setEvents(_event_data_source);
    }
}

void EventTableModel::setGroupFilter(int group_id) {
    _filtered_group_id = group_id;
    _applyGroupFilter();
}

void EventTableModel::clearGroupFilter() {
    setGroupFilter(-1);
}

int EventTableModel::rowCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(_display_data.size());
}

int EventTableModel::columnCount(QModelIndex const & parent) const {
    Q_UNUSED(parent);
    return 2;// Frame, Group
}

QVariant EventTableModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (index.row() >= static_cast<int>(_display_data.size())) {
        return QVariant{};
    }

    EventTableRow const & row = _display_data.at(index.row());

    switch (index.column()) {
        case 0:
            return QString::number(row.time.getValue());
        case 1:
            return QVariant::fromValue(row.group_name);
        default:
            return QVariant{};
    }
}

QVariant EventTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant{};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Frame");
            case 1:
                return QString("Group");
            default:
                return QVariant{};
        }
    }

    return QVariant{};
}

Qt::ItemFlags EventTableModel::flags(QModelIndex const & index) const {
    if (!index.isValid()) {
        return Qt::ItemIsEnabled;
    }
    return QAbstractTableModel::flags(index);
}

EventTableRow EventTableModel::getRowData(int row) const {
    if (row >= 0 && row < static_cast<int>(_display_data.size())) {
        return _display_data[row];
    }
    return EventTableRow{TimeFrameIndex(-1), EntityId(0), "Invalid"};
}

TimeFrameIndex EventTableModel::getEvent(int row) const {
    if (row >= 0 && row < static_cast<int>(_display_data.size())) {
        return _display_data[row].time;
    }
    return TimeFrameIndex(-1);
}

void EventTableModel::_applyGroupFilter() {
    beginResetModel();
    _display_data.clear();

    if (_filtered_group_id == -1) {
        _display_data = _all_data;
    } else {
        for (auto const & row: _all_data) {
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
