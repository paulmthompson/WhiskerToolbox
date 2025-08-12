#pragma once

#include "SelectionManager.hpp"
#include <vector>
#include <unordered_set>
#include <cstdint>

class PointData;
class MaskData;
class LineData;

/**
 * Selection adapter for scatter plot data (vector<float> x_data, vector<float> y_data)
 * Implements SelectionDataAdapter for simple vector-based point data
 */
class ScatterPlotSelectionAdapter : public SelectionDataAdapter {
public:
    ScatterPlotSelectionAdapter(const std::vector<float>& x_data, 
                               const std::vector<float>& y_data);

    // SelectionDataAdapter interface
    void applySelection(const std::vector<size_t>& indices) override;
    std::vector<size_t> getSelectedIndices() const override;
    void clearSelection() override;
    
    void assignSelectedToGroup(int group_id) override;
    void removeSelectedFromGroups() override;
    
    void hideSelected() override;
    void showAll() override;
    
    size_t getTotalSelected() const override;
    bool isPointSelected(size_t index) const override;
    
    size_t getPointCount() const override;
    std::pair<float, float> getPointPosition(size_t index) const override;

    // ScatterPlot specific
    void setGroupManager(GroupManager* group_manager);
    const std::unordered_set<int64_t>& getSelectedPointIds() const { return _selected_points; }
    const std::unordered_set<int64_t>& getVisiblePointIds() const { return _visible_points; }

private:
    const std::vector<float>& _x_data;
    const std::vector<float>& _y_data;
    std::unordered_set<int64_t> _selected_points;
    std::unordered_set<int64_t> _visible_points;
    GroupManager* _group_manager = nullptr;
    
    void ensureCorrectSize();
};

/**
 * Selection adapter for event plot data (vector<vector<float>>)
 * Each inner vector represents events for one trial
 */
class EventPlotSelectionAdapter : public SelectionDataAdapter {
public:
    explicit EventPlotSelectionAdapter(const std::vector<std::vector<float>>& event_data);

    // SelectionDataAdapter interface  
    void applySelection(const std::vector<size_t>& indices) override;
    std::vector<size_t> getSelectedIndices() const override;
    void clearSelection() override;
    
    void assignSelectedToGroup(int group_id) override;
    void removeSelectedFromGroups() override;
    
    void hideSelected() override;
    void showAll() override;
    
    size_t getTotalSelected() const override;
    bool isPointSelected(size_t index) const override;
    
    size_t getPointCount() const override;
    std::pair<float, float> getPointPosition(size_t index) const override;

    // EventPlot specific
    void setGroupManager(GroupManager* group_manager);
    std::pair<size_t, size_t> getTrialAndEventIndex(size_t flat_index) const;

private:
    const std::vector<std::vector<float>>& _event_data;
    std::unordered_set<int64_t> _selected_events;
    std::unordered_set<int64_t> _visible_events;
    GroupManager* _group_manager = nullptr;
    
    size_t _total_events = 0;
    std::vector<size_t> _trial_offsets;  // Cumulative event counts for indexing
    
    void buildIndexMapping();
    void ensureCorrectSize();
    int64_t flatIndexToEventId(size_t flat_index) const;
    size_t eventIdToFlatIndex(int64_t event_id) const;
};

/**
 * Selection adapter for spatial overlay data (points, masks, lines)
 * Handles selection across multiple data types and datasets
 */
class SpatialOverlaySelectionAdapter : public SelectionDataAdapter {
public:
    SpatialOverlaySelectionAdapter(
        const std::unordered_map<QString, std::shared_ptr<PointData>>& point_data,
        const std::unordered_map<QString, std::shared_ptr<MaskData>>& mask_data,
        const std::unordered_map<QString, std::shared_ptr<LineData>>& line_data);

    // SelectionDataAdapter interface
    void applySelection(const std::vector<size_t>& indices) override;
    std::vector<size_t> getSelectedIndices() const override;
    void clearSelection() override;
    
    void assignSelectedToGroup(int group_id) override;
    void removeSelectedFromGroups() override;
    
    void hideSelected() override;
    void showAll() override;
    
    size_t getTotalSelected() const override;
    bool isPointSelected(size_t index) const override;
    
    size_t getPointCount() const override;
    std::pair<float, float> getPointPosition(size_t index) const override;

    // SpatialOverlay specific
    void setGroupManager(GroupManager* group_manager);
    
    struct ElementInfo {
        enum Type { Point, Mask, Line };
        Type type;
        QString dataset_key;
        size_t element_index;
    };
    
    ElementInfo getElementInfo(size_t flat_index) const;

private:
    const std::unordered_map<QString, std::shared_ptr<PointData>>& _point_data;
    const std::unordered_map<QString, std::shared_ptr<MaskData>>& _mask_data;
    const std::unordered_map<QString, std::shared_ptr<LineData>>& _line_data;
    
    std::unordered_set<int64_t> _selected_elements;
    std::unordered_set<int64_t> _visible_elements;
    GroupManager* _group_manager = nullptr;
    
    size_t _total_elements = 0;
    std::vector<ElementInfo> _element_mapping;  // Maps flat index to element info
    
    void buildElementMapping();
    void ensureCorrectSize();
    int64_t flatIndexToElementId(size_t flat_index) const;
};
