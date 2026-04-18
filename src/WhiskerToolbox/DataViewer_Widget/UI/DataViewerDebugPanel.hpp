/**
 * @file DataViewerDebugPanel.hpp
 * @brief Developer diagnostics panel for the DataViewer widget
 *
 * Provides real-time visibility into the CorePlotting/PlottingOpenGL pipeline
 * including lane layout, rendering statistics, and coordinate transforms.
 * Gated behind DataViewerState::developerMode().
 */

#ifndef DATAVIEWER_DEBUG_PANEL_HPP
#define DATAVIEWER_DEBUG_PANEL_HPP

#include <QWidget>

#include <memory>

class DataViewerState;
class OpenGLWidget;
class QCheckBox;
class QGroupBox;
class QLabel;
class QTableWidget;

/**
 * @brief Developer diagnostics panel showing layout, rendering, and coordinate info
 *
 * Organized into collapsible QGroupBox sections:
 *  - Global View State (time window, Y bounds, zoom/scale)
 *  - Lane Table (one row per registered series)
 *  - Rendering Statistics (batch counts, vertex counts)
 *  - Coordinate Inspector (mouse position in multiple spaces)
 *  - Overlay Toggles (lane boundaries, origin markers, crosshair)
 */
class DataViewerDebugPanel : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the debug panel
     * @param state Shared viewer state (for view state queries)
     * @param opengl_widget Non-owning pointer to the OpenGL canvas
     * @param parent Parent widget
     */
    explicit DataViewerDebugPanel(
            std::shared_ptr<DataViewerState> state,
            OpenGLWidget * opengl_widget,
            QWidget * parent = nullptr);

    ~DataViewerDebugPanel() override = default;

public slots:
    /// Refresh all sections from current diagnostics
    void refresh();

    /**
     * @brief Update the coordinate inspector from a mouse hover event
     * @param time_coord Time coordinate (X axis)
     * @param canvas_y Canvas Y position in pixels
     * @param series_info Series identification string (or empty)
     */
    void updateCoordinateInspector(float time_coord, float canvas_y,
                                   QString const & series_info);

private:
    std::shared_ptr<DataViewerState> _state;
    OpenGLWidget * _opengl_widget = nullptr;///< Non-owning

    // --- UI sections ---
    QGroupBox * _view_state_group = nullptr;
    QGroupBox * _lane_table_group = nullptr;
    QGroupBox * _rendering_stats_group = nullptr;
    QGroupBox * _coordinate_group = nullptr;
    QGroupBox * _overlay_group = nullptr;

    // --- View state labels ---
    QLabel * _time_window_label = nullptr;
    QLabel * _visible_duration_label = nullptr;
    QLabel * _y_bounds_label = nullptr;
    QLabel * _pan_zoom_label = nullptr;
    QLabel * _widget_size_label = nullptr;
    QLabel * _aspect_ratio_label = nullptr;

    // --- Lane table ---
    QTableWidget * _lane_table = nullptr;

    // --- Rendering stats labels ---
    QLabel * _batch_counts_label = nullptr;
    QLabel * _vertex_count_label = nullptr;
    QLabel * _entity_count_label = nullptr;
    QLabel * _rebuild_count_label = nullptr;

    // --- Coordinate inspector labels ---
    QLabel * _canvas_pos_label = nullptr;
    QLabel * _ndc_pos_label = nullptr;
    QLabel * _world_pos_label = nullptr;
    QLabel * _data_pos_label = nullptr;
    QLabel * _series_under_cursor_label = nullptr;

    // --- Overlay toggle checkboxes ---
    QCheckBox * _lane_boundaries_cb = nullptr;
    QCheckBox * _origin_markers_cb = nullptr;
    QCheckBox * _crosshair_cb = nullptr;

    void _buildUI();
    void _connectSignals();
    void _refreshViewState();
    void _refreshLaneTable();
    void _refreshRenderingStats();
};

#endif// DATAVIEWER_DEBUG_PANEL_HPP
