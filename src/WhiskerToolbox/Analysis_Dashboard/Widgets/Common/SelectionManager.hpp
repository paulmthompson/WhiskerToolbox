#pragma once

#include "Selection/SelectionHandlers.hpp"
#include "Selection/SelectionModes.hpp"

#include <QObject>

#include <vector>
#include <memory>
#include <functional>
#include <optional>

class GroupManager;

/**
 * Abstract adapter for handling selection operations on different data structures
 * Uses Adapter pattern to decouple selection logic from data storage format
 */
class SelectionDataAdapter {
public:
    virtual ~SelectionDataAdapter() = default;
    
    // Core selection operations
    virtual void applySelection(const std::vector<size_t>& indices) = 0;
    virtual std::vector<size_t> getSelectedIndices() const = 0;
    virtual void clearSelection() = 0;
    
    // Group operations
    virtual void assignSelectedToGroup(int group_id) = 0;
    virtual void removeSelectedFromGroups() = 0;
    
    // Visibility operations
    virtual void hideSelected() = 0;
    virtual void showAll() = 0;
    
    // Query operations
    virtual size_t getTotalSelected() const = 0;
    virtual bool isPointSelected(size_t index) const = 0;
    
    // Point iteration for hit testing
    virtual size_t getPointCount() const = 0;
    virtual std::pair<float, float> getPointPosition(size_t index) const = 0;
    
    // Group manager integration
    virtual void setGroupManager(GroupManager* group_manager) = 0;
};

/**
 * Manages selection operations and coordinates between handlers and data adapters
 * Uses Strategy pattern for different selection modes
 */
class SelectionManager : public QObject {
    Q_OBJECT

public:

    explicit SelectionManager(QObject* parent = nullptr);
    virtual ~SelectionManager();

    // Setup
    void setSelectionHandler(SelectionVariant handler);
    void setDataAdapter(std::unique_ptr<SelectionDataAdapter> adapter);
    void setSelectionMode(SelectionMode mode);
    void setGroupManager(GroupManager* group_manager);

    // Selection operations
    void makeSelection();
    void applySelectionFromHandler(const SelectionVariant& handler);
    void clearSelection();
    void selectAll();


    // Group operations  
    void assignSelectedToNewGroup();
    void assignSelectedToGroup(int group_id);
    void ungroupSelected();

    // Visibility operations
    void hideSelected();
    void showAll();

    // Query
    size_t getTotalSelected() const;
    SelectionMode getCurrentMode() const { return _current_mode; }

    // Hit testing for tooltips/hover
    std::optional<size_t> findPointNear(float world_x, float world_y, float tolerance) const;

    // Get selection Adapter
    SelectionDataAdapter* getDataAdapter() const {
        return _data_adapter.get();
    }

signals:
    void selectionChanged(size_t total_selected);
    void selectionModeChanged(SelectionMode mode);

private:
    SelectionMode _current_mode = SelectionMode::PointSelection;
    SelectionVariant _current_handler;
    std::unique_ptr<SelectionDataAdapter> _data_adapter;
    GroupManager* _group_manager = nullptr;

    void createHandlerForMode(SelectionMode mode);
    std::vector<size_t> getSelectedIndicesFromHandler(const SelectionVariant& handler) const;
};
