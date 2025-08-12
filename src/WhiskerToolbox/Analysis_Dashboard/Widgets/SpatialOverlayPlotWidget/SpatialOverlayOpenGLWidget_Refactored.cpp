#include "SpatialOverlayOpenGLWidget_Refactored.hpp"
#include "SpatialOverlayViewAdapter.hpp"
#include "../Common/SelectionManager.hpp"
#include "../Common/PlotSelectionAdapters.hpp"
#include "../Common/PlotInteractionController.hpp"

// Include existing visualization structs
#include "Visualizers/Points/PointDataVisualization.hpp"
#include "Visualizers/Masks/MaskDataVisualization.hpp"
#include "Visualizers/Lines/LineDataVisualization.hpp"

// Forward declarations for data types - these need to be included/declared properly
class PointData;
class MaskData;
class LineData;

#include <QDebug>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <algorithm>
#include <limits>

SpatialOverlayOpenGLWidget::SpatialOverlayOpenGLWidget(QWidget* parent)
    : BasePlotOpenGLWidget(parent)
    , _data_bounds_valid(false),
    _data_bounds({0.0f, 0.0f, 0.0f, 0.0f}) {
    
    // Initialize context menu
    initializeContextMenu();
    
    qDebug() << "SpatialOverlayOpenGLWidget: Created with composition-based design";
}

SpatialOverlayOpenGLWidget::~SpatialOverlayOpenGLWidget() {
    // Cleanup is handled by smart pointers and base class
}

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
        
        if (_selection_manager) {
            _selection_manager->setSelectionMode(mode);
        }
        
        emit selectionModeChanged(mode);
        updateContextMenuState();
    }
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedPoints() const {
    if (!_selection_manager) {
        return 0;
    }
    
    // Get the spatial overlay adapter and count selected points by type
    auto adapter = dynamic_cast<SpatialOverlaySelectionAdapter*>(_selection_manager->getDataAdapter());
    if (!adapter) {
        return 0;
    }
    
    size_t point_count = 0;
    auto selected_indices = adapter->getSelectedIndices();
    for (size_t index : selected_indices) {
        auto element_info = adapter->getElementInfo(index);
        if (element_info.type == SpatialOverlaySelectionAdapter::ElementInfo::Point) {
            point_count++;
        }
    }
    return point_count;
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedMasks() const {
    if (!_selection_manager) {
        return 0;
    }
    
    // Get the spatial overlay adapter and count selected masks by type
    auto adapter = dynamic_cast<SpatialOverlaySelectionAdapter*>(_selection_manager->getDataAdapter());
    if (!adapter) {
        return 0;
    }
    
    size_t mask_count = 0;
    auto selected_indices = adapter->getSelectedIndices();
    for (size_t index : selected_indices) {
        auto element_info = adapter->getElementInfo(index);
        if (element_info.type == SpatialOverlaySelectionAdapter::ElementInfo::Mask) {
            mask_count++;
        }
    }
    return mask_count;
}

size_t SpatialOverlayOpenGLWidget::getTotalSelectedLines() const {
    if (!_selection_manager) {
        return 0;
    }
    
    // Get the spatial overlay adapter and count selected lines by type
    auto adapter = dynamic_cast<SpatialOverlaySelectionAdapter*>(_selection_manager->getDataAdapter());
    if (!adapter) {
        return 0;
    }
    
    size_t line_count = 0;
    auto selected_indices = adapter->getSelectedIndices();
    for (size_t index : selected_indices) {
        auto element_info = adapter->getElementInfo(index);
        if (element_info.type == SpatialOverlaySelectionAdapter::ElementInfo::Line) {
            line_count++;
        }
    }
    return line_count;
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
    _context_menu = std::make_unique<QMenu>(this);
    
    // Add basic context menu actions
    auto* hideAction = _context_menu->addAction("Hide Selected");
    connect(hideAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::hideSelectedItems);
    
    auto* showAllCurrentAction = _context_menu->addAction("Show All (Current Dataset)");
    connect(showAllCurrentAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::showAllItemsCurrentDataset);
    
    auto* showAllAction = _context_menu->addAction("Show All (All Datasets)");
    connect(showAllAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::showAllItemsAllDatasets);
    
    _context_menu->addSeparator();
    
    auto* resetViewAction = _context_menu->addAction("Reset View");
    connect(resetViewAction, &QAction::triggered, this, &SpatialOverlayOpenGLWidget::resetView);
}

void SpatialOverlayOpenGLWidget::updateContextMenuState() {
    // TODO: Enable/disable menu items based on current selection state
}

void SpatialOverlayOpenGLWidget::mouseMoveEvent(QMouseEvent* event) {
    // Call base class implementation first (handles interaction controller and tooltips)
    BasePlotOpenGLWidget::mouseMoveEvent(event);
    
    // Add hover logic for point enlargement
    if (_tooltips_enabled && _opengl_resources_initialized) {
        auto world_pos = screenToWorld(event->pos().x(), event->pos().y());
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
