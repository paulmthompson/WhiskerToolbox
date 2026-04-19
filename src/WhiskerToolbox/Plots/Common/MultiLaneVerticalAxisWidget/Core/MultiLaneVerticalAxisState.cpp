/**
 * @file MultiLaneVerticalAxisState.cpp
 * @brief Implementation of MultiLaneVerticalAxisState
 */

#include "MultiLaneVerticalAxisState.hpp"

#include <utility>

MultiLaneVerticalAxisState::MultiLaneVerticalAxisState(QObject * parent)
    : QObject(parent) {
}

void MultiLaneVerticalAxisState::setLanes(std::vector<LaneAxisDescriptor> lanes) {
    _data.lanes = std::move(lanes);
    emit lanesChanged();
}

void MultiLaneVerticalAxisState::setShowLabels(bool show) {
    if (_data.show_labels != show) {
        _data.show_labels = show;
        emit visibilityChanged();
    }
}

void MultiLaneVerticalAxisState::setShowSeparators(bool show) {
    if (_data.show_separators != show) {
        _data.show_separators = show;
        emit visibilityChanged();
    }
}
