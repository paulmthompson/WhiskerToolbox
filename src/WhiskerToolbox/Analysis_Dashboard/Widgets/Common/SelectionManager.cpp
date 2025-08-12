#include "SelectionManager.hpp"
#include "Groups/GroupManager.hpp"
#include "Selection/PointSelectionHandler.hpp"
#include "Selection/LineSelectionHandler.hpp"
#include "Selection/PolygonSelectionHandler.hpp"
#include "Selection/NoneSelectionHandler.hpp"
#include "CoreGeometry/points.hpp"

#include <QDebug>
#include <QString>

SelectionManager::SelectionManager(QObject* parent)
    : QObject(parent)
    , _current_mode(SelectionMode::PointSelection)
    , _group_manager(nullptr) {
    
    qDebug() << "SelectionManager: Created";
}

SelectionManager::~SelectionManager() = default;

void SelectionManager::setSelectionHandler(SelectionVariant handler) {
    _current_handler = std::move(handler);
    
    // Set up notification callback if the handler supports it
    // Note: This assumes the handlers have a setNotificationCallback method
    // We'll need to standardize this interface
    qDebug() << "SelectionManager: Set selection handler";
}

void SelectionManager::setDataAdapter(std::unique_ptr<SelectionDataAdapter> adapter) {
    _data_adapter = std::move(adapter);
    
    // If we have a group manager, set it on the adapter
    if (_data_adapter && _group_manager) {
        _data_adapter->setGroupManager(_group_manager);
    }
    
    qDebug() << "SelectionManager: Set data adapter with" << _data_adapter->getPointCount() << "points";
}

void SelectionManager::setGroupManager(GroupManager* group_manager) {
    _group_manager = group_manager;
    
    // If we have a data adapter, set the group manager on it
    if (_data_adapter) {
        _data_adapter->setGroupManager(group_manager);
    }
    
    qDebug() << "SelectionManager: Set group manager";
}

void SelectionManager::setSelectionMode(SelectionMode mode) {
    if (mode != _current_mode) {
        _current_mode = mode;
        createHandlerForMode(mode);
        emit selectionModeChanged(mode);
        qDebug() << "SelectionManager: Changed selection mode to" << static_cast<int>(mode);
    }
}

void SelectionManager::makeSelection() {
    if (!_data_adapter) {
        qWarning() << "SelectionManager::makeSelection: No data adapter set";
        return;
    }
    
    // For now, we'll implement a basic selection mechanism
    // This will be enhanced when we integrate with the actual selection handlers
    
    // TODO: Get selected indices from current handler
    std::vector<size_t> selected_indices;
    
    _data_adapter->applySelection(selected_indices);
    emit selectionChanged(selected_indices.size());
    
    qDebug() << "SelectionManager::makeSelection: Applied selection of" << selected_indices.size() << "points";
}

void SelectionManager::applySelectionFromHandler(const SelectionVariant& handler) {
    if (!_data_adapter) {
        qWarning() << "SelectionManager::applySelectionFromHandler: No data adapter set";
        return;
    }
    
    // Get selected indices based on the handler type and its selection region
    std::vector<size_t> selected_indices = getSelectedIndicesFromHandler(handler);
    
    _data_adapter->applySelection(selected_indices);
    emit selectionChanged(selected_indices.size());
    
    qDebug() << "SelectionManager::applySelectionFromHandler: Applied selection of" << selected_indices.size() << "points";
}

void SelectionManager::clearSelection() {
    if (!_data_adapter) {
        return;
    }
    
    _data_adapter->clearSelection();
    emit selectionChanged(0);
    
    qDebug() << "SelectionManager: Cleared selection";
}

void SelectionManager::selectAll() {
    if (!_data_adapter) {
        return;
    }
    
    std::vector<size_t> all_indices;
    size_t count = _data_adapter->getPointCount();
    all_indices.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        all_indices.push_back(i);
    }
    
    _data_adapter->applySelection(all_indices);
    emit selectionChanged(count);
    
    qDebug() << "SelectionManager: Selected all" << count << "points";
}

void SelectionManager::assignSelectedToNewGroup() {
    if (!_data_adapter || !_group_manager) {
        return;
    }
    
    // Create new group and assign selected points
    int new_group_id = _group_manager->createGroup(QString::number(new_group_id));
    _data_adapter->assignSelectedToGroup(new_group_id);
    
    qDebug() << "SelectionManager: Assigned selected points to new group" << new_group_id;
}

void SelectionManager::assignSelectedToGroup(int group_id) {
    if (!_data_adapter) {
        return;
    }
    
    _data_adapter->assignSelectedToGroup(group_id);
    
    qDebug() << "SelectionManager: Assigned selected points to group" << group_id;
}

void SelectionManager::ungroupSelected() {
    if (!_data_adapter) {
        return;
    }
    
    _data_adapter->removeSelectedFromGroups();
    
    qDebug() << "SelectionManager: Removed selected points from groups";
}

void SelectionManager::hideSelected() {
    if (!_data_adapter) {
        return;
    }
    
    _data_adapter->hideSelected();
    
    qDebug() << "SelectionManager: Hid selected points";
}

void SelectionManager::showAll() {
    if (!_data_adapter) {
        return;
    }
    
    _data_adapter->showAll();
    
    qDebug() << "SelectionManager: Showed all points";
}

size_t SelectionManager::getTotalSelected() const {
    if (!_data_adapter) {
        return 0;
    }
    
    return _data_adapter->getTotalSelected();
}

std::optional<size_t> SelectionManager::findPointNear(float world_x, float world_y, float tolerance) const {
    if (!_data_adapter) {
        return std::nullopt;
    }
    
    size_t count = _data_adapter->getPointCount();
    float tolerance_sq = tolerance * tolerance;
    
    for (size_t i = 0; i < count; ++i) {
        auto [px, py] = _data_adapter->getPointPosition(i);
        float dx = px - world_x;
        float dy = py - world_y;
        float dist_sq = dx * dx + dy * dy;
        
        if (dist_sq <= tolerance_sq) {
            return i;
        }
    }
    
    return std::nullopt;
}

void SelectionManager::createHandlerForMode(SelectionMode mode) {
    // TODO: Create appropriate handler based on mode
    // For now, we'll delay this until we have the selection handler integration
    
    switch (mode) {
        case SelectionMode::None:
            // _current_handler = std::make_unique<NoneSelectionHandler>();
            break;
        case SelectionMode::PointSelection:
            // _current_handler = std::make_unique<PointSelectionHandler>(10.0f);
            break;
        case SelectionMode::LineIntersection:
            // _current_handler = std::make_unique<LineIntersectionHandler>();
            break;
        case SelectionMode::PolygonSelection:
            // _current_handler = std::make_unique<PolygonSelectionHandler>();
            break;
    }
    
    qDebug() << "SelectionManager: Created handler for mode" << static_cast<int>(mode);
}

std::vector<size_t> SelectionManager::getSelectedIndicesFromHandler(const SelectionVariant& handler) const {
    std::vector<size_t> selected_indices;
    
    if (!_data_adapter) {
        return selected_indices;
    }
    
    std::visit([&](auto const& h) {
        using HandlerType = std::decay_t<decltype(h)>;
        if constexpr (std::is_same_v<HandlerType, std::unique_ptr<PointSelectionHandler>>) {
            // For point selection, find the point near the selection position
            if (h) {
                QVector2D world_pos = h->getWorldPos();
                float tolerance = h->getWorldTolerance();
                
                auto point_index = findPointNear(world_pos.x(), world_pos.y(), tolerance);
                if (point_index.has_value()) {
                    selected_indices.push_back(point_index.value());
                }
            }
        } else if constexpr (std::is_same_v<HandlerType, std::unique_ptr<PolygonSelectionHandler>>) {
            // For polygon selection, check which points are inside the polygon
            if (h && h->getActiveSelectionRegion()) {
                size_t count = _data_adapter->getPointCount();
                for (size_t i = 0; i < count; ++i) {
                    auto [px, py] = _data_adapter->getPointPosition(i);
                    if (h->getActiveSelectionRegion()->containsPoint(Point2D<float>(px, py))) {
                        selected_indices.push_back(i);
                    }
                }
            }
        } else if constexpr (std::is_same_v<HandlerType, std::unique_ptr<LineSelectionHandler>>) {
            // For line selection, check which points intersect the line
            if (h && h->getActiveSelectionRegion()) {
                size_t count = _data_adapter->getPointCount();
                for (size_t i = 0; i < count; ++i) {
                    auto [px, py] = _data_adapter->getPointPosition(i);
                    if (h->getActiveSelectionRegion()->containsPoint(Point2D<float>(px, py))) {
                        selected_indices.push_back(i);
                    }
                }
            }
        }
        // NoneSelectionHandler returns empty selection
    }, handler);
    
    return selected_indices;
}
