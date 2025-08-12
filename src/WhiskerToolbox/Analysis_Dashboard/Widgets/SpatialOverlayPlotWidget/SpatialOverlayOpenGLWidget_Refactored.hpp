#pragma once

#include "../Common/BasePlotOpenGLWidget.hpp"
#include "../Common/PlotSelectionAdapters.hpp"
#include "Selection/SelectionModes.hpp"
#include <QString>
#include <memory>
#include <unordered_map>

class PointData;
class MaskData;
class LineData;
struct PointDataVisualization;
struct MaskDataVisualization;
struct LineDataVisualization;

/**
 * Refactored SpatialOverlayOpenGLWidget using composition-based design
 * Inherits common functionality from BasePlotOpenGLWidget
 * 
 * This replaces the original "god class" by breaking down responsibilities:
 * - Data visualization handled by individual PointDataVisualization, MaskDataVisualization, LineDataVisualization
 * - Selection handled by SelectionManager (from base)
 * - Tooltips handled by TooltipManager (from base)
 * - Interaction handled by PlotInteractionController (from base)
 */
class SpatialOverlayOpenGLWidget : public BasePlotOpenGLWidget {
    Q_OBJECT

public:
    explicit SpatialOverlayOpenGLWidget(QWidget* parent = nullptr);
    ~SpatialOverlayOpenGLWidget() override;

    // Data management
    void setPointData(std::unordered_map<QString, std::shared_ptr<PointData>> const& point_data_map);
    void setMaskData(std::unordered_map<QString, std::shared_ptr<MaskData>> const& mask_data_map);
    void setLineData(std::unordered_map<QString, std::shared_ptr<LineData>> const& line_data_map);

    // Rendering properties
    void setLineWidth(float line_width);
    float getLineWidth() const { return _line_width; }

    // Time filtering
    void applyTimeRangeFilter(int start_frame, int end_frame);

    // Selection mode (extends base selection)
    void setSelectionMode(SelectionMode mode);
    SelectionMode getSelectionMode() const { return _selection_mode; }

    // Selection queries
    size_t getTotalSelectedPoints() const;
    size_t getTotalSelectedMasks() const;
    size_t getTotalSelectedLines() const;

    // Visibility management
    void hideSelectedItems();
    void showAllItemsCurrentDataset();
    void showAllItemsAllDatasets();

    // Coordinate conversion (public interface)
    QVector2D screenToWorld(int screen_x, int screen_y) const;
    QPoint worldToScreen(float world_x, float world_y) const;

signals:
    void frameJumpRequested(int64_t time_frame_index, QString const& data_key);
    void lineWidthChanged(float line_width);
    void selectionModeChanged(SelectionMode mode);
    void pointSizeChanged(float point_size);
    void tooltipsEnabledChanged(bool enabled);

protected:
    // BasePlotOpenGLWidget interface implementation
    void renderData() override;
    void calculateDataBounds() override;
    BoundingBox getDataBounds() const override;
    std::unique_ptr<SelectionManager> createSelectionManager() override;

    // OpenGL lifecycle
    void initializeGL() override;

    // Optional overrides
    void renderUI() override;
    std::optional<QString> generateTooltipContent(QPoint const& screen_pos) const override;

    // Context menu support
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onSelectionChanged(size_t total_selected);

private:
    // Data visualizations - using existing visualization structs
    std::unordered_map<QString, std::unique_ptr<PointDataVisualization>> _point_data_visualizations;
    std::unordered_map<QString, std::unique_ptr<MaskDataVisualization>> _mask_data_visualizations;
    std::unordered_map<QString, std::unique_ptr<LineDataVisualization>> _line_data_visualizations;
    
    // Selection mode
    SelectionMode _selection_mode = SelectionMode::None;
    
    // Time filtering
    int _start_frame = -1;
    int _end_frame = -1;
    
    // Data storage
    std::unordered_map<QString, std::shared_ptr<PointData>> _point_data;
    std::unordered_map<QString, std::shared_ptr<MaskData>> _mask_data;
    std::unordered_map<QString, std::shared_ptr<LineData>> _line_data;
    
    // Cached bounds
    BoundingBox _data_bounds;
    bool _data_bounds_valid = false;
    
    // Context menu
    std::unique_ptr<QMenu> _context_menu;

    void initializeVisualizations();
    void updateVisualizationData();
    void initializeContextMenu();
    void updateContextMenuState();

    friend class SpatialOverlayViewAdapter;
};
