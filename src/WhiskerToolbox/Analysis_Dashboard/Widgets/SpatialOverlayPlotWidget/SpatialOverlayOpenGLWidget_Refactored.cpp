#include "SpatialOverlayOpenGLWidget_Refactored.hpp"
#include "SpatialOverlayViewAdapter.hpp"
#include "../Common/SelectionManager.hpp"
#include "../Common/PlotSelectionAdapters.hpp"
#include "../Common/PlotInteractionController.hpp"
#include "Selection/SelectionHandlers.hpp"
#include "Selection/PointSelectionHandler.hpp"
#include "Selection/PolygonSelectionHandler.hpp"
#include "Selection/LineSelectionHandler.hpp"
#include "Selection/NoneSelectionHandler.hpp"

#include "Visualizers/Points/PointDataVisualization.hpp"
#include "Visualizers/Masks/MaskDataVisualization.hpp"
#include "Visualizers/Lines/LineDataVisualization.hpp"

#include <QDebug>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>

#include <algorithm>
#include <limits>

class PointData;
class MaskData;
class LineData;

SpatialOverlayOpenGLWidget::SpatialOverlayOpenGLWidget(QWidget* parent)
    : BasePlotOpenGLWidget(parent)
    , _data_bounds_valid(false),
    _data_bounds({0.0f, 0.0f, 0.0f, 0.0f}) {
    
    // Initialize default selection handler
    createSelectionHandler(_selection_mode);
    
    // Initialize context menu
    initializeContextMenu();
    
    qDebug() << "SpatialOverlayOpenGLWidget: Created with composition-based design";
}

SpatialOverlayOpenGLWidget::~SpatialOverlayOpenGLWidget() = default;

void SpatialOverlayOpenGLWidget::initializeGL() {
    // Call base class initialization first
    BasePlotOpenGLWidget::initializeGL();
    
    // Initialize interaction controller with spatial overlay view adapter
    if (!_interaction) {
        auto adapter = std::make_unique<SpatialOverlayViewAdapter>(this);
        _interaction = std::make_unique<PlotInteractionController>(this, std::move(adapter));
        
        // Connect interaction signals
        connect(_interaction.get(), &PlotInteractionController::viewBoundsChanged,
                this, &SpatialOverlayOpenGLWidget::viewBoundsChanged);
        connect(_interaction.get(), &PlotInteractionController::mouseWorldMoved,
                this, &SpatialOverlayOpenGLWidget::mouseWorldMoved);
    }
    
    // Initialize visualizations
    initializeVisualizations();
}

void SpatialOverlayOpenGLWidget::setPointData(std::unordered_map<QString, std::shared_ptr<PointData>> const& point_data_map) {
    qDebug() << "SpatialOverlayOpenGLWidget::setPointData called with" << point_data_map.size() << "datasets";

    _point_data = point_data_map;
    
    // Clear existing visualizations
    _point_data_visualizations.clear();
    
    // Create new visualizations
    if (_opengl_resources_initialized) {
        makeCurrent();
        for (auto const& [key, point_data] : _point_data) {
            if (point_data) {
                auto viz = std::make_unique<PointDataVisualization>(key, point_data, _group_manager);
                _point_data_visualizations[key] = std::move(viz);
            }
        }
        doneCurrent();
    }

    // Calculate bounds
    calculateDataBounds();

    // Ensure selection manager is created/updated
    ensureSelectionManager();

    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::setMaskData(std::unordered_map<QString, std::shared_ptr<MaskData>> const& mask_data_map) {
    qDebug() << "SpatialOverlayOpenGLWidget::setMaskData called with" << mask_data_map.size() << "datasets";

    _mask_data = mask_data_map;
    
    // Clear existing visualizations
    _mask_data_visualizations.clear();
    
    // Create new visualizations
    if (_opengl_resources_initialized) {
        makeCurrent();
        for (auto const& [key, mask_data] : _mask_data) {
            if (mask_data) {
                auto viz = std::make_unique<MaskDataVisualization>(key, mask_data);
                _mask_data_visualizations[key] = std::move(viz);
            }
        }
        doneCurrent();
    }

    calculateDataBounds();
    
    // Ensure selection manager is created/updated
    ensureSelectionManager();
    
    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::setLineData(std::unordered_map<QString, std::shared_ptr<LineData>> const& line_data_map) {
    qDebug() << "SpatialOverlayOpenGLWidget::setLineData called with" << line_data_map.size() << "datasets";

    _line_data = line_data_map;
    
    // Clear existing visualizations
    _line_data_visualizations.clear();
    
    // Create new visualizations
    if (_opengl_resources_initialized) {
        makeCurrent();
        for (auto const& [key, line_data] : _line_data) {
            if (line_data) {
                auto viz = std::make_unique<LineDataVisualization>(key, line_data);
                _line_data_visualizations[key] = std::move(viz);
            }
        }
        doneCurrent();
    }

    calculateDataBounds();
    
    // Ensure selection manager is created/updated
    ensureSelectionManager();
    
    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::setLineWidth(float line_width) {
    float new_line_width = std::max(1.0f, std::min(20.0f, line_width));
    if (new_line_width != _line_width) {
        _line_width = new_line_width;
        emit lineWidthChanged(_line_width);
        requestThrottledUpdate();
    }
}

void SpatialOverlayOpenGLWidget::applyTimeRangeFilter(int start_frame, int end_frame) {
    _start_frame = start_frame;
    _end_frame = end_frame;
    
    // Apply time filtering to existing visualizations
    for (auto& [key, viz] : _point_data_visualizations) {
        if (viz) {
            //viz->applyTimeRangeFilter(start_frame, end_frame);
        }
    }
    
    for (auto& [key, viz] : _mask_data_visualizations) {
        if (viz) {
            //viz->applyTimeRangeFilter(start_frame, end_frame);
        }
    }
    
    for (auto& [key, viz] : _line_data_visualizations) {
        if (viz) {
            //viz->applyTimeRangeFilter(start_frame, end_frame);
        }
    }
    
    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::setSelectionMode(SelectionMode mode) {
    if (_selection_mode != mode) {
        _selection_mode = mode;
        
        // Create appropriate selection handler
        createSelectionHandler(mode);
        
        if (_selection_manager) {
            _selection_manager->setSelectionMode(mode);
        }
        
        emit selectionModeChanged(mode);
        updateContextMenuState();
    }
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedPoints() const {
    size_t total = 0;
    for (auto const & [key, viz]: _point_data_visualizations) {
        if (!viz) continue;
        total += viz->m_selected_points.size();
    }
    return total;
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedMasks() const {
    size_t total = 0;
    for (auto const & [key, viz]: _mask_data_visualizations) {
        if (!viz) continue;
        total += viz->selected_masks.size();
    }
    return total;
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedLines() const {
    size_t total = 0;
    for (auto const & [key, viz]: _line_data_visualizations) {
        if (!viz) continue;
        total += viz->m_selected_lines.size();
    }
    return total;
}

void SpatialOverlayOpenGLWidget::clearSelection() {
    bool had_selection = false;

    for (auto const & [key, viz]: _point_data_visualizations) {
        if (!viz->m_selected_points.empty()) {
            viz->clearSelection();
            had_selection = true;
        }
    }

    for (auto const & [key, viz]: _mask_data_visualizations) {
        if (!viz->selected_masks.empty()) {
            viz->clearSelection();
            had_selection = true;
        }
    }

    for (auto const & [key, viz]: _line_data_visualizations) {
        if (!viz->m_selected_lines.empty()) {
            viz->clearSelection();
            had_selection = true;
        }
    }

    if (had_selection) {
        size_t total_selected = getTotalSelectedPoints() + getTotalSelectedMasks() + getTotalSelectedLines();
        emit selectionChanged(total_selected, QString(), 0);
        requestThrottledUpdate();
        
        qDebug() << "SpatialOverlayOpenGLWidget: Selection cleared";
    }
}

void SpatialOverlayOpenGLWidget::hideSelectedItems() {
    // TODO: Implement by updating visualization visibility
    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::showAllItemsCurrentDataset() {
    // TODO: Implement by updating visualization visibility
    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::showAllItemsAllDatasets() {
    // TODO: Implement by updating visualization visibility
    requestThrottledUpdate();
}

QVector2D SpatialOverlayOpenGLWidget::screenToWorld(int screen_x, int screen_y) const {
    return BasePlotOpenGLWidget::screenToWorld(QPoint(screen_x, screen_y));
}

QPoint SpatialOverlayOpenGLWidget::worldToScreen(float world_x, float world_y) const {
    return BasePlotOpenGLWidget::worldToScreen(world_x, world_y);
}

void SpatialOverlayOpenGLWidget::renderData() {
    auto context = createRenderingContext();
    auto mvp_matrix = context.projection_matrix * context.view_matrix * context.model_matrix;

    // Render in order: masks (background), lines (middle), points (foreground)
    for (auto const& [key, viz] : _mask_data_visualizations) {
        if (viz) {
            viz->render(mvp_matrix);
        }
    }
    
    for (auto const& [key, viz] : _line_data_visualizations) {
        if (viz) {
            viz->render(mvp_matrix, _line_width);
        }
    }
    
    for (auto const& [key, viz] : _point_data_visualizations) {
        if (viz) {
            viz->render(mvp_matrix, _point_size);
        }
    }
}

void SpatialOverlayOpenGLWidget::renderOverlays() {
    // Render selection handlers (polygon outlines, rubber band lines, etc.)
    auto context = createRenderingContext();
    auto mvp_matrix = context.projection_matrix * context.view_matrix * context.model_matrix;
    
    std::visit([mvp_matrix](auto & handler) {
        if (handler) {
            handler->render(mvp_matrix);
        }
    }, _selection_handler);
    
    // Call base implementation for any additional overlay rendering
    BasePlotOpenGLWidget::renderOverlays();
}

void SpatialOverlayOpenGLWidget::calculateDataBounds() {
    // Reset bounds
    _data_bounds = BoundingBox{0.0f, 0.0f, 0.0f, 0.0f};
    _data_bounds_valid = false;

    if (_point_data_visualizations.empty() && _mask_data_visualizations.empty() && _line_data_visualizations.empty()) {
        return;
    }

    bool has_data = false;
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    // Calculate bounds from point data visualizations
    for (auto const& [key, viz] : _point_data_visualizations) {
        if (viz && viz->m_visible && !viz->m_vertex_data.empty()) {
            // Iterate through vertex data (x, y, group_id triplets)
            for (size_t i = 0; i < viz->m_vertex_data.size(); i += 3) {
                float x = viz->m_vertex_data[i];
                float y = viz->m_vertex_data[i + 1];
                // Skip group_id at i + 2

                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
                has_data = true;
            }
        }
    }

    // Calculate bounds from mask data visualizations
    for (auto const& [key, viz] : _mask_data_visualizations) {
        if (viz && viz->visible) {
            min_x = std::min(min_x, viz->world_min_x);
            max_x = std::max(max_x, viz->world_max_x);
            min_y = std::min(min_y, viz->world_min_y);
            max_y = std::max(max_y, viz->world_max_y);
            has_data = true;
        }
    }

    // Calculate bounds from line data visualizations
    for (auto const& [key, viz] : _line_data_visualizations) {
        if (viz && viz->m_visible && !viz->m_vertex_data.empty()) {
            // Iterate through line vertex data (x, y pairs)
            for (size_t i = 0; i < viz->m_vertex_data.size(); i += 2) {
                float x = viz->m_vertex_data[i];
                float y = viz->m_vertex_data[i + 1];

                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
                has_data = true;
            }
        }
    }

    if (has_data) {
        _data_bounds = BoundingBox{min_x, min_y, max_x, max_y};
        _data_bounds_valid = true;
    }

    qDebug() << "SpatialOverlayOpenGLWidget: Calculated data bounds:"
             << _data_bounds.min_x << _data_bounds.min_y 
             << _data_bounds.max_x << _data_bounds.max_y;
}

BoundingBox SpatialOverlayOpenGLWidget::getDataBounds() const {
    return _data_bounds;
}

std::unique_ptr<SelectionManager> SpatialOverlayOpenGLWidget::createSelectionManager() {
    auto manager = std::make_unique<SelectionManager>();
    
    // Create and set adapter for spatial overlay data
    auto adapter = std::make_unique<SpatialOverlaySelectionAdapter>(_point_data, _mask_data, _line_data);
    manager->setDataAdapter(std::move(adapter));
    
    // Connect selection change signal
    connect(manager.get(), &SelectionManager::selectionChanged,
            this, &SpatialOverlayOpenGLWidget::onSelectionChanged);
    
    return manager;
}

void SpatialOverlayOpenGLWidget::ensureSelectionManager() {
    // Create selection manager if it doesn't exist
    if (!_selection_manager) {
        _selection_manager = createSelectionManager();
        if (_selection_manager && _group_manager) {
            _selection_manager->setGroupManager(_group_manager);
        }
    } else {
        // Update existing selection manager with new data
        auto adapter = std::make_unique<SpatialOverlaySelectionAdapter>(_point_data, _mask_data, _line_data);
        _selection_manager->setDataAdapter(std::move(adapter));
    }
}

void SpatialOverlayOpenGLWidget::renderUI() {
    // TODO: Render axis labels, legends, etc.
    // This can include drawing text overlays, coordinate system indicators, etc.
}

std::optional<QString> SpatialOverlayOpenGLWidget::generateTooltipContent(QPoint const& screen_pos) const {
    if (_point_data_visualizations.empty() && _mask_data_visualizations.empty() && _line_data_visualizations.empty() || !_tooltips_enabled) {
        return std::nullopt;
    }
    
    // Convert screen position to world coordinates
    auto world_pos = screenToWorld(screen_pos.x(), screen_pos.y());
    float world_x = world_pos.x();
    float world_y = world_pos.y();
    
    // Query visualizations for closest data element
    // This would ideally use the hit testing methods of the visualizations
    // For now, provide basic coordinate information
    
    QString tooltip = QString("Position: (%1, %2)")
                        .arg(world_x, 0, 'f', 3)
                        .arg(world_y, 0, 'f', 3);
    
    // Add data counts if available
    if (!_point_data_visualizations.empty() || !_mask_data_visualizations.empty() || !_line_data_visualizations.empty()) {
        tooltip += QString("\nData: %1 points, %2 masks, %3 lines")
                    .arg(_point_data_visualizations.size())
                    .arg(_mask_data_visualizations.size())
                    .arg(_line_data_visualizations.size());
    }
    
    return tooltip;
}

void SpatialOverlayOpenGLWidget::contextMenuEvent(QContextMenuEvent* event) {
    if (_context_menu) {
        updateContextMenuState();
        _context_menu->popup(event->globalPos());
    }
}

void SpatialOverlayOpenGLWidget::onSelectionChanged(size_t total_selected) {
    // Emit signals for compatibility with existing code
    emit selectionChanged(total_selected, QString(), 0);
    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::initializeVisualizations() {
    // Visualizations are created when data is set
    // This method can be used for any common initialization
    qDebug() << "SpatialOverlayOpenGLWidget: Initialized visualizations";
}

void SpatialOverlayOpenGLWidget::updateVisualizationData() {
    // Data is updated through individual visualization creation
    // This method can be used for updating existing visualizations
    qDebug() << "SpatialOverlayOpenGLWidget: Updated visualization data";
}

void SpatialOverlayOpenGLWidget::initializeContextMenu() {
    _context_menu = std::make_unique<QMenu>(nullptr);
    
    // Create actions
    _action_create_new_group = new QAction("Create New Group", this);
    connect(_action_create_new_group, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::assignSelectedPointsToNewGroup);
    
    _action_ungroup_selected = new QAction("Ungroup Selected", this);
    connect(_action_ungroup_selected, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::ungroupSelectedPoints);
    
    _action_hide_selected = new QAction("Hide Selected", this);
    connect(_action_hide_selected, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::hideSelectedItems);
    
    _action_show_all_current = new QAction("Show All (Current Dataset)", this);
    connect(_action_show_all_current, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::showAllItemsCurrentDataset);
    
    _action_show_all_datasets = new QAction("Show All (All Datasets)", this);
    connect(_action_show_all_datasets, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::showAllItemsAllDatasets);
    
    // Create assign to group submenu
    _assign_group_menu = _context_menu->addMenu("Assign to Group");
    _assign_group_menu->addAction(_action_create_new_group);
    _assign_group_menu->addSeparator();
    // Dynamic group actions will be added by updateDynamicGroupActions()
    
    // Add other menu items
    _context_menu->addAction(_action_ungroup_selected);
    _context_menu->addSeparator();
    _context_menu->addAction(_action_hide_selected);
    
    // Show All submenu
    QMenu* showAllMenu = _context_menu->addMenu("Show All");
    showAllMenu->addAction(_action_show_all_current);
    showAllMenu->addAction(_action_show_all_datasets);
    
    _context_menu->addSeparator();
    
    auto* resetViewAction = _context_menu->addAction("Reset View");
    connect(resetViewAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::resetView);
}

void SpatialOverlayOpenGLWidget::updateContextMenuState() {
    size_t total_selected = getTotalSelectedPoints() + getTotalSelectedMasks() + getTotalSelectedLines();
    bool has_selection = total_selected > 0;
    bool has_group_manager = _group_manager != nullptr;
    
    // Update group-related actions
    _assign_group_menu->menuAction()->setVisible(has_selection && has_group_manager);
    _action_ungroup_selected->setVisible(has_selection && has_group_manager);
    
    // Update other actions
    _action_hide_selected->setEnabled(has_selection);
    
    // Update dynamic group actions if we have a group manager
    if (has_group_manager && has_selection) {
        updateDynamicGroupActions();
    }
}

void SpatialOverlayOpenGLWidget::mousePressEvent(QMouseEvent* event) {
    // Ensure this widget gets keyboard focus so Enter/Escape reach selection handlers
    if (!hasFocus()) {
        setFocus(Qt::MouseFocusReason);
    }
    
    // Convert to world coordinates for selection handlers
    auto world_pos = screenToWorld(event->pos().x(), event->pos().y());

    // Delegate to selection handler using std::visit
    std::visit([event, world_pos](auto & handler) {
        if (handler) {
            handler->mousePressEvent(event, world_pos);
        }
    }, _selection_handler);

    // Call base class implementation for interaction controller
    BasePlotOpenGLWidget::mousePressEvent(event);
    
    // Accept the event
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        event->accept();
    } else {
        event->ignore();
    }
}

void SpatialOverlayOpenGLWidget::mouseReleaseEvent(QMouseEvent* event) {
    // Convert to world coordinates for selection handlers
    auto world_pos = screenToWorld(event->pos().x(), event->pos().y());

    // Delegate to selection handler using std::visit
    std::visit([event, world_pos](auto & handler) {
        if (handler) {
            handler->mouseReleaseEvent(event, world_pos);
        }
    }, _selection_handler);

    // For point selection, apply the selection on release
    if (_selection_mode == SelectionMode::PointSelection && event->button() == Qt::LeftButton) {
        makeSelection();
    }

    // Call base class implementation
    BasePlotOpenGLWidget::mouseReleaseEvent(event);
    
    event->accept();
}

void SpatialOverlayOpenGLWidget::mouseMoveEvent(QMouseEvent* event) {
    // Convert to world coordinates for selection handlers
    auto world_pos = screenToWorld(event->pos().x(), event->pos().y());

    // Delegate to selection handler using std::visit
    std::visit([event, world_pos](auto & handler) {
        if (handler) {
            handler->mouseMoveEvent(event, world_pos);
        }
    }, _selection_handler);

    // Call base class implementation first (handles interaction controller and tooltips)
    BasePlotOpenGLWidget::mouseMoveEvent(event);
    
    // Add hover logic for point enlargement
    if (_tooltips_enabled && _opengl_resources_initialized) {
        float tolerance = 10.0f; // 10 pixel tolerance for hover detection
        
        bool hover_changed = false;
        
        // Handle hover for all point visualizations
        for (auto const& [key, viz] : _point_data_visualizations) {
            if (viz && viz->handleHover(world_pos, tolerance)) {
                hover_changed = true;
            }
        }
        
        // Request update if hover state changed (to redraw enlarged point)
        if (hover_changed) {
            requestThrottledUpdate();
        }
    }
}

void SpatialOverlayOpenGLWidget::makeSelection() {
    auto context = createRenderingContext();

    std::cout << "makeSelection" << std::endl;

    if (_selection_handler.valueless_by_exception()) {
        return;
    }

    // Determine if we should clear selection. For point selection, there is no region
    // concept, so we should NOT clear. Only clear when selection mode is None or when
    // region-based handlers have no active region.
    bool should_clear = false;
    if (_selection_mode == SelectionMode::None) {
        should_clear = true;
    } else {
        should_clear = std::visit([](auto & handler) {
            using HandlerType = std::decay_t<decltype(handler)>;
            if constexpr (std::is_same_v<HandlerType, std::unique_ptr<PointSelectionHandler>>) {
                return false;// point selection never uses a persistent region
            } else if constexpr (std::is_same_v<HandlerType, std::unique_ptr<NoneSelectionHandler>>) {
                return true;
            } else {
                return handler->getActiveSelectionRegion() == nullptr;
            }
        },
                                  _selection_handler);
    }

    if (should_clear) {
        clearSelection();
        return;
    }

    // Apply selection to all visualization structs using their existing applySelection methods
    for (auto const & [key, viz]: _point_data_visualizations) {
        viz->applySelection(_selection_handler);
    }
    for (auto const & [key, viz]: _mask_data_visualizations) {
        viz->applySelection(_selection_handler);
    }
    for (auto const & [key, viz]: _line_data_visualizations) {
        viz->applySelection(_selection_handler, context);
    }

    // Emit selection changed signal with updated counts
    size_t total_selected = getTotalSelectedPoints() + getTotalSelectedMasks() + getTotalSelectedLines();
    emit selectionChanged(total_selected, QString(), 0);

    requestThrottledUpdate();
}

void SpatialOverlayOpenGLWidget::createSelectionHandler(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::None:
            _selection_handler = std::make_unique<NoneSelectionHandler>();
            break;
        case SelectionMode::PointSelection:
            _selection_handler = std::make_unique<PointSelectionHandler>(10.0f); // 10 pixel tolerance
            break;
        case SelectionMode::PolygonSelection:
            _selection_handler = std::make_unique<PolygonSelectionHandler>();
            break;
        case SelectionMode::LineIntersection:
            _selection_handler = std::make_unique<LineSelectionHandler>();
            break;
    }
    
    // Set up notification callback for the handler
    std::visit([this](auto & handler) {
        if (handler) {
            handler->setNotificationCallback([this]() { makeSelection(); });
        }
    }, _selection_handler);
}

void SpatialOverlayOpenGLWidget::updateDynamicGroupActions() {
    // Clear existing dynamic actions
    for (QAction* action : _dynamic_group_actions) {
        _assign_group_menu->removeAction(action);
        action->deleteLater();
    }
    _dynamic_group_actions.clear();

    if (!_group_manager) {
        return;
    }

    auto const& groups = _group_manager->getGroups();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        auto const& group = it.value();
        QAction* groupAction = _assign_group_menu->addAction(group.name);
        int group_id = group.id;
        connect(groupAction, &QAction::triggered, this, [this, group_id]() {
            assignSelectedPointsToGroup(group_id);
        });
        _dynamic_group_actions.append(groupAction);
    }
}

void SpatialOverlayOpenGLWidget::assignSelectedPointsToNewGroup() {
    if (!_group_manager) {
        qDebug() << "SpatialOverlayOpenGLWidget: No group manager available for group assignment";
        return;
    }

    // Collect all selected point IDs
    std::unordered_set<int64_t> selected_point_ids;
    for (auto const& [key, viz] : _point_data_visualizations) {
        if (viz) {
            auto point_ids = viz->getSelectedPointIds();
            selected_point_ids.insert(point_ids.begin(), point_ids.end());
        }
    }

    if (selected_point_ids.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: No selected points to assign to new group";
        return;
    }

    // Create a new group
    QString group_name = QString("Group %1").arg(_group_manager->getGroups().size() + 1);
    int group_id = _group_manager->createGroup(group_name);

    // Assign selected points to the new group
    _group_manager->assignPointsToGroup(group_id, selected_point_ids);

    // Clear selection after assignment
    clearSelection();

    qDebug() << "SpatialOverlayOpenGLWidget: Assigned" << selected_point_ids.size()
             << "points to new group" << group_id;

    // Update the context menu to reflect the new group
    updateDynamicGroupActions();
}

void SpatialOverlayOpenGLWidget::assignSelectedPointsToGroup(int group_id) {
    if (!_group_manager) {
        qDebug() << "SpatialOverlayOpenGLWidget: No group manager available for group assignment";
        return;
    }

    // Collect all selected point IDs
    std::unordered_set<int64_t> selected_point_ids;
    for (auto const& [key, viz] : _point_data_visualizations) {
        if (viz) {
            auto point_ids = viz->getSelectedPointIds();
            selected_point_ids.insert(point_ids.begin(), point_ids.end());
        }
    }

    if (selected_point_ids.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: No selected points to assign to group";
        return;
    }

    // Assign selected points to the specified group
    _group_manager->assignPointsToGroup(group_id, selected_point_ids);

    // Clear selection after assignment
    clearSelection();

    qDebug() << "SpatialOverlayOpenGLWidget: Assigned" << selected_point_ids.size()
             << "points to group" << group_id;
}

void SpatialOverlayOpenGLWidget::ungroupSelectedPoints() {
    if (!_group_manager) {
        qDebug() << "SpatialOverlayOpenGLWidget: No group manager available for ungrouping";
        return;
    }

    // Collect all selected point IDs
    std::unordered_set<int64_t> selected_point_ids;
    for (auto const& [key, viz] : _point_data_visualizations) {
        if (viz) {
            auto point_ids = viz->getSelectedPointIds();
            selected_point_ids.insert(point_ids.begin(), point_ids.end());
        }
    }

    if (selected_point_ids.empty()) {
        qDebug() << "SpatialOverlayOpenGLWidget: No selected points to ungroup";
        return;
    }

    // Remove selected points from their groups
    _group_manager->ungroupPoints(selected_point_ids);

    // Clear selection after ungrouping
    clearSelection();

    qDebug() << "SpatialOverlayOpenGLWidget: Ungrouped" << selected_point_ids.size() << "points";

    // Update the context menu to reflect group changes
    updateDynamicGroupActions();
}
