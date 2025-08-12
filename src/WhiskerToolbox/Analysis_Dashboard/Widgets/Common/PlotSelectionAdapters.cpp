#include "PlotSelectionAdapters.hpp"
#include "Groups/GroupManager.hpp"

#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include <QDebug>
#include <algorithm>

// ============================================================================
// ScatterPlotSelectionAdapter Implementation
// ============================================================================

ScatterPlotSelectionAdapter::ScatterPlotSelectionAdapter(const std::vector<float>& x_data, 
                                                        const std::vector<float>& y_data)
    : _x_data(x_data)
    , _y_data(y_data)
    , _group_manager(nullptr) {
    
    ensureCorrectSize();
    qDebug() << "ScatterPlotSelectionAdapter: Created for" << _x_data.size() << "points";
}

void ScatterPlotSelectionAdapter::applySelection(const std::vector<size_t>& indices) {
    // Clear current selection
    _selected_points.clear();
    
    // Apply new selection - convert size_t indices to int64_t point IDs
    for (size_t index : indices) {
        if (index < _x_data.size()) {
            _selected_points.insert(static_cast<int64_t>(index));
        }
    }
    
    qDebug() << "ScatterPlotSelectionAdapter: Applied selection to" << indices.size() << "points";
}

std::vector<size_t> ScatterPlotSelectionAdapter::getSelectedIndices() const {
    std::vector<size_t> indices;
    indices.reserve(_selected_points.size());
    
    for (int64_t point_id : _selected_points) {
        if (point_id >= 0 && static_cast<size_t>(point_id) < _x_data.size()) {
            indices.push_back(static_cast<size_t>(point_id));
        }
    }
    
    return indices;
}

void ScatterPlotSelectionAdapter::clearSelection() {
    _selected_points.clear();
}

void ScatterPlotSelectionAdapter::assignSelectedToGroup(int group_id) {
    if (!_group_manager) {
        qWarning() << "ScatterPlotSelectionAdapter::assignSelectedToGroup: No group manager set";
        return;
    }
    
    if (_selected_points.empty()) {
        qDebug() << "ScatterPlotSelectionAdapter: No points selected to assign to group";
        return;
    }
    
    _group_manager->assignPointsToGroup(group_id, _selected_points);
    
    qDebug() << "ScatterPlotSelectionAdapter: Assigned" << _selected_points.size() 
             << "points to group" << group_id;
}

void ScatterPlotSelectionAdapter::removeSelectedFromGroups() {
    if (!_group_manager) {
        return;
    }
    
    if (_selected_points.empty()) {
        return;
    }
    
    _group_manager->ungroupPoints(_selected_points);
    
    qDebug() << "ScatterPlotSelectionAdapter: Removed" << _selected_points.size() 
             << "points from groups";
}

void ScatterPlotSelectionAdapter::hideSelected() {
    for (int64_t point_id : _selected_points) {
        _visible_points.erase(point_id);
    }
    
    qDebug() << "ScatterPlotSelectionAdapter: Hid" << _selected_points.size() << "points";
}

void ScatterPlotSelectionAdapter::showAll() {
    // Rebuild visible points set with all points
    _visible_points.clear();
    for (size_t i = 0; i < _x_data.size(); ++i) {
        _visible_points.insert(static_cast<int64_t>(i));
    }
    
    qDebug() << "ScatterPlotSelectionAdapter: Showed all" << _visible_points.size() << "points";
}

size_t ScatterPlotSelectionAdapter::getTotalSelected() const {
    return _selected_points.size();
}

bool ScatterPlotSelectionAdapter::isPointSelected(size_t index) const {
    return _selected_points.count(static_cast<int64_t>(index)) > 0;
}

size_t ScatterPlotSelectionAdapter::getPointCount() const {
    return _x_data.size();
}

std::pair<float, float> ScatterPlotSelectionAdapter::getPointPosition(size_t index) const {
    if (index >= _x_data.size()) {
        return {0.0f, 0.0f};
    }
    return {_x_data[index], _y_data[index]};
}

void ScatterPlotSelectionAdapter::setGroupManager(GroupManager* group_manager) {
    _group_manager = group_manager;
}

void ScatterPlotSelectionAdapter::ensureCorrectSize() {
    // Initialize all points as visible
    _visible_points.clear();
    for (size_t i = 0; i < _x_data.size(); ++i) {
        _visible_points.insert(static_cast<int64_t>(i));
    }
}

// ============================================================================
// EventPlotSelectionAdapter Implementation
// ============================================================================

EventPlotSelectionAdapter::EventPlotSelectionAdapter(const std::vector<std::vector<float>>& event_data)
    : _event_data(event_data)
    , _group_manager(nullptr)
    , _total_events(0) {
    
    buildIndexMapping();
    ensureCorrectSize();
    
    qDebug() << "EventPlotSelectionAdapter: Created for" << _event_data.size() 
             << "trials with" << _total_events << "total events";
}

void EventPlotSelectionAdapter::applySelection(const std::vector<size_t>& indices) {
    // Clear current selection
    _selected_events.clear();
    
    // Apply new selection - convert flat indices to event IDs
    for (size_t index : indices) {
        if (index < _total_events) {
            int64_t event_id = flatIndexToEventId(index);
            _selected_events.insert(event_id);
        }
    }
    
    qDebug() << "EventPlotSelectionAdapter: Applied selection to" << indices.size() << "events";
}

std::vector<size_t> EventPlotSelectionAdapter::getSelectedIndices() const {
    std::vector<size_t> indices;
    indices.reserve(_selected_events.size());
    
    for (int64_t event_id : _selected_events) {
        size_t flat_index = eventIdToFlatIndex(event_id);
        if (flat_index < _total_events) {
            indices.push_back(flat_index);
        }
    }
    
    return indices;
}

void EventPlotSelectionAdapter::clearSelection() {
    _selected_events.clear();
}

void EventPlotSelectionAdapter::assignSelectedToGroup(int group_id) {
    if (!_group_manager) {
        qWarning() << "EventPlotSelectionAdapter::assignSelectedToGroup: No group manager set";
        return;
    }
    
    if (_selected_events.empty()) {
        qDebug() << "EventPlotSelectionAdapter: No events selected to assign to group";
        return;
    }
    
    _group_manager->assignPointsToGroup(group_id, _selected_events);
    
    qDebug() << "EventPlotSelectionAdapter: Assigned" << _selected_events.size()
             << "events to group" << group_id;
}

void EventPlotSelectionAdapter::removeSelectedFromGroups() {
    if (!_group_manager) {
        return;
    }
    
    if (_selected_events.empty()) {
        return;
    }
    
    _group_manager->ungroupPoints(_selected_events);
    
    qDebug() << "EventPlotSelectionAdapter: Removed" << _selected_events.size() 
             << "events from groups";
}

void EventPlotSelectionAdapter::hideSelected() {
    for (int64_t event_id : _selected_events) {
        _visible_events.erase(event_id);
    }
    
    qDebug() << "EventPlotSelectionAdapter: Hid" << _selected_events.size() << "events";
}

void EventPlotSelectionAdapter::showAll() {
    // Rebuild visible events set with all events
    _visible_events.clear();
    for (size_t i = 0; i < _total_events; ++i) {
        int64_t event_id = flatIndexToEventId(i);
        _visible_events.insert(event_id);
    }
    
    qDebug() << "EventPlotSelectionAdapter: Showed all" << _visible_events.size() << "events";
}

size_t EventPlotSelectionAdapter::getTotalSelected() const {
    return _selected_events.size();
}

bool EventPlotSelectionAdapter::isPointSelected(size_t index) const {
    if (index >= _total_events) {
        return false;
    }
    
    int64_t event_id = flatIndexToEventId(index);
    return _selected_events.count(event_id) > 0;
}

size_t EventPlotSelectionAdapter::getPointCount() const {
    return _total_events;
}

std::pair<float, float> EventPlotSelectionAdapter::getPointPosition(size_t index) const {
    if (index >= _total_events) {
        return {0.0f, 0.0f};
    }
    
    auto [trial_idx, event_idx] = getTrialAndEventIndex(index);
    if (trial_idx >= _event_data.size() || event_idx >= _event_data[trial_idx].size()) {
        return {0.0f, 0.0f};
    }
    
    float x = _event_data[trial_idx][event_idx];  // Event time
    float y = static_cast<float>(trial_idx);      // Trial index
    
    return {x, y};
}

void EventPlotSelectionAdapter::setGroupManager(GroupManager* group_manager) {
    _group_manager = group_manager;
}

std::pair<size_t, size_t> EventPlotSelectionAdapter::getTrialAndEventIndex(size_t flat_index) const {
    if (flat_index >= _total_events) {
        return {0, 0};
    }
    
    // Find which trial this event belongs to
    for (size_t trial = 0; trial < _trial_offsets.size() - 1; ++trial) {
        if (flat_index >= _trial_offsets[trial] && flat_index < _trial_offsets[trial + 1]) {
            size_t event_in_trial = flat_index - _trial_offsets[trial];
            return {trial, event_in_trial};
        }
    }
    
    return {0, 0};
}

void EventPlotSelectionAdapter::buildIndexMapping() {
    _total_events = 0;
    _trial_offsets.clear();
    _trial_offsets.push_back(0);
    
    for (const auto& trial : _event_data) {
        _total_events += trial.size();
        _trial_offsets.push_back(_total_events);
    }
}

void EventPlotSelectionAdapter::ensureCorrectSize() {
    // Initialize all events as visible
    _visible_events.clear();
    for (size_t i = 0; i < _total_events; ++i) {
        int64_t event_id = flatIndexToEventId(i);
        _visible_events.insert(event_id);
    }
}

int64_t EventPlotSelectionAdapter::flatIndexToEventId(size_t flat_index) const {
    // For now, use a simple mapping: event_id = flat_index
    // In a more sophisticated implementation, this could map to actual timestamp IDs
    return static_cast<int64_t>(flat_index);
}

size_t EventPlotSelectionAdapter::eventIdToFlatIndex(int64_t event_id) const {
    // For now, use a simple reverse mapping: flat_index = event_id
    // This assumes event_id is a valid flat index
    if (event_id >= 0 && static_cast<size_t>(event_id) < _total_events) {
        return static_cast<size_t>(event_id);
    }
    return 0;
}

// ============================================================================
// SpatialOverlaySelectionAdapter Implementation
// ============================================================================

SpatialOverlaySelectionAdapter::SpatialOverlaySelectionAdapter(
    const std::unordered_map<QString, std::shared_ptr<PointData>>& point_data,
    const std::unordered_map<QString, std::shared_ptr<MaskData>>& mask_data,
    const std::unordered_map<QString, std::shared_ptr<LineData>>& line_data)
    : _point_data(point_data)
    , _mask_data(mask_data)
    , _line_data(line_data)
    , _group_manager(nullptr)
    , _total_elements(0) {
    
    buildElementMapping();
    ensureCorrectSize();
    
    qDebug() << "SpatialOverlaySelectionAdapter: Created for" 
             << point_data.size() << "point datasets,"
             << mask_data.size() << "mask datasets,"
             << line_data.size() << "line datasets with"
             << _total_elements << "total elements";
}

void SpatialOverlaySelectionAdapter::applySelection(const std::vector<size_t>& indices) {
    // Clear current selection
    _selected_elements.clear();
    
    // Apply new selection - convert flat indices to element IDs
    for (size_t index : indices) {
        if (index < _total_elements) {
            int64_t element_id = flatIndexToElementId(index);
            _selected_elements.insert(element_id);
        }
    }
    
    qDebug() << "SpatialOverlaySelectionAdapter: Applied selection to" << indices.size() << "elements";
}

std::vector<size_t> SpatialOverlaySelectionAdapter::getSelectedIndices() const {
    std::vector<size_t> indices;
    indices.reserve(_selected_elements.size());
    
    for (int64_t element_id : _selected_elements) {
        // Convert element_id back to flat index
        if (element_id >= 0 && static_cast<size_t>(element_id) < _total_elements) {
            indices.push_back(static_cast<size_t>(element_id));
        }
    }
    
    return indices;
}

void SpatialOverlaySelectionAdapter::clearSelection() {
    _selected_elements.clear();
}

void SpatialOverlaySelectionAdapter::assignSelectedToGroup(int group_id) {
    if (!_group_manager) {
        qWarning() << "SpatialOverlaySelectionAdapter::assignSelectedToGroup: No group manager set";
        return;
    }
    
    if (_selected_elements.empty()) {
        qDebug() << "SpatialOverlaySelectionAdapter: No elements selected to assign to group";
        return;
    }
    
    _group_manager->assignPointsToGroup(group_id, _selected_elements);
    
    qDebug() << "SpatialOverlaySelectionAdapter: Assigned" << _selected_elements.size()
             << "elements to group" << group_id;
}

void SpatialOverlaySelectionAdapter::removeSelectedFromGroups() {
    if (!_group_manager) {
        return;
    }
    
    if (_selected_elements.empty()) {
        return;
    }
    
    _group_manager->ungroupPoints(_selected_elements);
    
    qDebug() << "SpatialOverlaySelectionAdapter: Removed" << _selected_elements.size()
             << "elements from groups";
}

void SpatialOverlaySelectionAdapter::hideSelected() {
    for (int64_t element_id : _selected_elements) {
        _visible_elements.erase(element_id);
    }
    
    qDebug() << "SpatialOverlaySelectionAdapter: Hid" << _selected_elements.size() << "elements";
}

void SpatialOverlaySelectionAdapter::showAll() {
    // Rebuild visible elements set with all elements
    _visible_elements.clear();
    for (size_t i = 0; i < _total_elements; ++i) {
        _visible_elements.insert(static_cast<int64_t>(i));
    }
    
    qDebug() << "SpatialOverlaySelectionAdapter: Showed all" << _visible_elements.size() << "elements";
}

size_t SpatialOverlaySelectionAdapter::getTotalSelected() const {
    return _selected_elements.size();
}

bool SpatialOverlaySelectionAdapter::isPointSelected(size_t index) const {
    return _selected_elements.count(static_cast<int64_t>(index)) > 0;
}

size_t SpatialOverlaySelectionAdapter::getPointCount() const {
    return _total_elements;
}

std::pair<float, float> SpatialOverlaySelectionAdapter::getPointPosition(size_t index) const {
    if (index >= _element_mapping.size()) {
        return {0.0f, 0.0f};
    }
    
    // TODO: Get actual position from the data element
    // This is a placeholder implementation
    const auto& element_info = _element_mapping[index];
    
    // For now, return a default position
    return {0.0f, 0.0f};
}

void SpatialOverlaySelectionAdapter::setGroupManager(GroupManager* group_manager) {
    _group_manager = group_manager;
}

SpatialOverlaySelectionAdapter::ElementInfo SpatialOverlaySelectionAdapter::getElementInfo(size_t flat_index) const {
    if (flat_index < _element_mapping.size()) {
        return _element_mapping[flat_index];
    }
    return {ElementInfo::Point, QString(), 0};
}

void SpatialOverlaySelectionAdapter::buildElementMapping() {
    _element_mapping.clear();
    _total_elements = 0;
    
    // Add points from all datasets
    for (const auto& [key, point_data] : _point_data) {
        if (point_data) {
            // TODO: Get actual point count from PointData interface
            // For now, assume each dataset has some points
            size_t point_count = 100; // Placeholder
            
            for (size_t i = 0; i < point_count; ++i) {
                _element_mapping.push_back({ElementInfo::Point, key, i});
                _total_elements++;
            }
        }
    }
    
    // Add masks from all datasets
    for (const auto& [key, mask_data] : _mask_data) {
        if (mask_data) {
            // TODO: Get actual mask count from MaskData interface
            size_t mask_count = 10; // Placeholder
            
            for (size_t i = 0; i < mask_count; ++i) {
                _element_mapping.push_back({ElementInfo::Mask, key, i});
                _total_elements++;
            }
        }
    }
    
    // Add lines from all datasets
    for (const auto& [key, line_data] : _line_data) {
        if (line_data) {
            // TODO: Get actual line count from LineData interface
            size_t line_count = 20; // Placeholder
            
            for (size_t i = 0; i < line_count; ++i) {
                _element_mapping.push_back({ElementInfo::Line, key, i});
                _total_elements++;
            }
        }
    }
}

void SpatialOverlaySelectionAdapter::ensureCorrectSize() {
    // Initialize all elements as visible
    _visible_elements.clear();
    for (size_t i = 0; i < _total_elements; ++i) {
        _visible_elements.insert(static_cast<int64_t>(i));
    }
}

int64_t SpatialOverlaySelectionAdapter::flatIndexToElementId(size_t flat_index) const {
    // For now, use a simple mapping: element_id = flat_index
    return static_cast<int64_t>(flat_index);
}
