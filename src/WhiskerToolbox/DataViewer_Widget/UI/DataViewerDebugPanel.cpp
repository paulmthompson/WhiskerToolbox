/**
 * @file DataViewerDebugPanel.cpp
 * @brief Implementation of the developer diagnostics panel for DataViewer
 */

#include "DataViewerDebugPanel.hpp"

#include "Core/DataViewerDiagnostics.hpp"
#include "Core/DataViewerState.hpp"
#include "Rendering/OpenGLWidget.hpp"

#include "CorePlotting/CoordinateTransform/InverseTransform.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

// ============================================================================
// Construction
// ============================================================================

DataViewerDebugPanel::DataViewerDebugPanel(
        std::shared_ptr<DataViewerState> state,
        OpenGLWidget * opengl_widget,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _opengl_widget(opengl_widget) {
    _buildUI();
    _connectSignals();
    refresh();
}

// ============================================================================
// Public slots
// ============================================================================

void DataViewerDebugPanel::refresh() {
    _refreshViewState();
    _refreshLaneTable();
    _refreshRenderingStats();
}

void DataViewerDebugPanel::updateCoordinateInspector(
        float time_coord, float canvas_y, QString const & series_info) {
    if (!_opengl_widget) {
        return;
    }

    auto const [w, h] = _opengl_widget->getCanvasSize();
    auto const & view = _state->viewState();

    // Reconstruct canvas_x from time_coord using time window
    float const time_frac =
            (view.x_max > view.x_min)
                    ? (time_coord - static_cast<float>(view.x_min)) /
                              static_cast<float>(view.x_max - view.x_min)
                    : 0.0f;
    float const canvas_x = time_frac * static_cast<float>(w);

    // NDC conversion
    auto const ndc = CorePlotting::canvasToNDC(canvas_x, canvas_y, w, h);

    // World coordinates (NDC Y with pan offset applied)
    float const world_x = time_coord;
    float const world_y = ndc.y + static_cast<float>(view.y_pan);

    _canvas_pos_label->setText(
            QStringLiteral("Canvas: (%1, %2) px")
                    .arg(static_cast<double>(canvas_x), 0, 'f', 1)
                    .arg(static_cast<double>(canvas_y), 0, 'f', 1));

    _ndc_pos_label->setText(
            QStringLiteral("NDC: (%1, %2)")
                    .arg(static_cast<double>(ndc.x), 0, 'f', 4)
                    .arg(static_cast<double>(ndc.y), 0, 'f', 4));

    _world_pos_label->setText(
            QStringLiteral("World: (%1, %2)")
                    .arg(static_cast<double>(world_x), 0, 'f', 2)
                    .arg(static_cast<double>(world_y), 0, 'f', 4));

    _data_pos_label->setText(
            QStringLiteral("Data: T=%1")
                    .arg(static_cast<double>(time_coord), 0, 'f', 1));

    _series_under_cursor_label->setText(
            series_info.isEmpty() ? QStringLiteral("Series: none")
                                  : QStringLiteral("Series: %1").arg(series_info));
}

// ============================================================================
// Private: UI construction
// ============================================================================

void DataViewerDebugPanel::_buildUI() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(6);

    // --- A.1 Global View State ---
    _view_state_group = new QGroupBox(QStringLiteral("View State"), this);
    _view_state_group->setCheckable(true);
    _view_state_group->setChecked(true);
    auto * vs_layout = new QVBoxLayout(_view_state_group);
    vs_layout->setContentsMargins(4, 4, 4, 4);
    vs_layout->setSpacing(2);

    _time_window_label = new QLabel(_view_state_group);
    _visible_duration_label = new QLabel(_view_state_group);
    _y_bounds_label = new QLabel(_view_state_group);
    _pan_zoom_label = new QLabel(_view_state_group);
    _widget_size_label = new QLabel(_view_state_group);
    _aspect_ratio_label = new QLabel(_view_state_group);

    for (auto * lbl: {_time_window_label, _visible_duration_label,
                      _y_bounds_label, _pan_zoom_label,
                      _widget_size_label, _aspect_ratio_label}) {
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setFont(QFont(QStringLiteral("monospace"), 8));
        vs_layout->addWidget(lbl);
    }

    main_layout->addWidget(_view_state_group);

    // --- A.2 Lane Table ---
    _lane_table_group = new QGroupBox(QStringLiteral("Lane Table"), this);
    _lane_table_group->setCheckable(true);
    _lane_table_group->setChecked(true);
    auto * lt_layout = new QVBoxLayout(_lane_table_group);
    lt_layout->setContentsMargins(4, 4, 4, 4);

    _lane_table = new QTableWidget(_lane_table_group);
    _lane_table->setColumnCount(12);
    _lane_table->setHorizontalHeaderLabels({
            QStringLiteral("Lane"),
            QStringLiteral("Key"),
            QStringLiteral("Type"),
            QStringLiteral("Visible"),
            QStringLiteral("Center Y"),
            QStringLiteral("Height"),
            QStringLiteral("Offset"),
            QStringLiteral("Gain"),
            QStringLiteral("User Scale"),
            QStringLiteral("Mean"),
            QStringLiteral("Std Dev"),
            QStringLiteral("On-Screen"),
    });
    _lane_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _lane_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _lane_table->setAlternatingRowColors(true);
    _lane_table->verticalHeader()->setVisible(false);
    _lane_table->horizontalHeader()->setStretchLastSection(true);
    _lane_table->setFont(QFont(QStringLiteral("monospace"), 8));
    _lane_table->setMinimumHeight(200);

    lt_layout->addWidget(_lane_table);
    main_layout->addWidget(_lane_table_group);

    // --- A.3 Rendering Statistics ---
    _rendering_stats_group = new QGroupBox(QStringLiteral("Rendering Stats"), this);
    _rendering_stats_group->setCheckable(true);
    _rendering_stats_group->setChecked(true);
    auto * rs_layout = new QVBoxLayout(_rendering_stats_group);
    rs_layout->setContentsMargins(4, 4, 4, 4);
    rs_layout->setSpacing(2);

    _batch_counts_label = new QLabel(_rendering_stats_group);
    _vertex_count_label = new QLabel(_rendering_stats_group);
    _entity_count_label = new QLabel(_rendering_stats_group);
    _rebuild_count_label = new QLabel(_rendering_stats_group);

    for (auto * lbl: {_batch_counts_label, _vertex_count_label,
                      _entity_count_label, _rebuild_count_label}) {
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setFont(QFont(QStringLiteral("monospace"), 8));
        rs_layout->addWidget(lbl);
    }

    main_layout->addWidget(_rendering_stats_group);

    // --- A.4 Coordinate Inspector ---
    _coordinate_group = new QGroupBox(QStringLiteral("Coordinate Inspector"), this);
    _coordinate_group->setCheckable(true);
    _coordinate_group->setChecked(true);
    auto * ci_layout = new QVBoxLayout(_coordinate_group);
    ci_layout->setContentsMargins(4, 4, 4, 4);
    ci_layout->setSpacing(2);

    _canvas_pos_label = new QLabel(QStringLiteral("Canvas: —"), _coordinate_group);
    _ndc_pos_label = new QLabel(QStringLiteral("NDC: —"), _coordinate_group);
    _world_pos_label = new QLabel(QStringLiteral("World: —"), _coordinate_group);
    _data_pos_label = new QLabel(QStringLiteral("Data: —"), _coordinate_group);
    _series_under_cursor_label = new QLabel(QStringLiteral("Series: —"), _coordinate_group);

    for (auto * lbl: {_canvas_pos_label, _ndc_pos_label,
                      _world_pos_label, _data_pos_label,
                      _series_under_cursor_label}) {
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setFont(QFont(QStringLiteral("monospace"), 8));
        ci_layout->addWidget(lbl);
    }

    main_layout->addWidget(_coordinate_group);

    // --- Overlay Toggles ---
    _overlay_group = new QGroupBox(QStringLiteral("Overlay Toggles"), this);
    auto * ov_layout = new QVBoxLayout(_overlay_group);
    ov_layout->setContentsMargins(4, 4, 4, 4);
    ov_layout->setSpacing(2);

    _lane_boundaries_cb = new QCheckBox(QStringLiteral("Lane Boundaries"), _overlay_group);
    _lane_boundaries_cb->setChecked(true);
    _origin_markers_cb = new QCheckBox(QStringLiteral("Origin Markers"), _overlay_group);
    _origin_markers_cb->setChecked(true);
    _crosshair_cb = new QCheckBox(QStringLiteral("Cursor Crosshair"), _overlay_group);
    _crosshair_cb->setChecked(true);

    ov_layout->addWidget(_lane_boundaries_cb);
    ov_layout->addWidget(_origin_markers_cb);
    ov_layout->addWidget(_crosshair_cb);

    main_layout->addWidget(_overlay_group);

    setLayout(main_layout);
}

// ============================================================================
// Private: Signal connections
// ============================================================================

void DataViewerDebugPanel::_connectSignals() {
    if (_opengl_widget) {
        connect(_opengl_widget, &OpenGLWidget::sceneRebuilt,
                this, &DataViewerDebugPanel::refresh);

        connect(_opengl_widget, &OpenGLWidget::mouseHover,
                this, &DataViewerDebugPanel::updateCoordinateInspector);

        // Wire overlay toggle checkboxes to OpenGLWidget setters
        connect(_lane_boundaries_cb, &QCheckBox::toggled,
                _opengl_widget, &OpenGLWidget::setOverlayLaneBoundaries);
        connect(_origin_markers_cb, &QCheckBox::toggled,
                _opengl_widget, &OpenGLWidget::setOverlayOriginMarkers);
        connect(_crosshair_cb, &QCheckBox::toggled,
                _opengl_widget, &OpenGLWidget::setOverlayCrosshair);
    }

    if (_state) {
        connect(_state.get(), &DataViewerState::viewStateChanged,
                this, &DataViewerDebugPanel::refresh);
    }
}

// ============================================================================
// Private: Section refreshers
// ============================================================================

void DataViewerDebugPanel::_refreshViewState() {
    if (!_state || !_opengl_widget) {
        return;
    }

    auto const & view = _state->viewState();
    auto const [w, h] = _opengl_widget->getCanvasSize();

    _time_window_label->setText(
            QStringLiteral("Time: [%1, %2]")
                    .arg(static_cast<int64_t>(view.x_min))
                    .arg(static_cast<int64_t>(view.x_max)));

    _visible_duration_label->setText(
            QStringLiteral("Duration: %1 frames")
                    .arg(static_cast<int64_t>(view.x_max - view.x_min)));

    float const eff_y_min = static_cast<float>(view.y_min) + static_cast<float>(view.y_pan);
    float const eff_y_max = static_cast<float>(view.y_max) + static_cast<float>(view.y_pan);
    _y_bounds_label->setText(
            QStringLiteral("Y: [%1, %2]  pan=%3")
                    .arg(static_cast<double>(eff_y_min), 0, 'f', 4)
                    .arg(static_cast<double>(eff_y_max), 0, 'f', 4)
                    .arg(view.y_pan, 0, 'f', 4));

    _pan_zoom_label->setText(
            QStringLiteral("y_scale=%1")
                    .arg(static_cast<double>(_state->globalYScale()), 0, 'f', 2));

    _widget_size_label->setText(
            QStringLiteral("Widget: %1 x %2 px").arg(w).arg(h));

    double const aspect =
            (h > 0) ? static_cast<double>(w) / static_cast<double>(h) : 0.0;
    _aspect_ratio_label->setText(
            QStringLiteral("Aspect: %1").arg(aspect, 0, 'f', 3));
}

void DataViewerDebugPanel::_refreshLaneTable() {
    if (!_opengl_widget) {
        return;
    }

    auto const diag = _opengl_widget->getDiagnostics();
    auto const & lanes = diag.lanes;

    _lane_table->setRowCount(static_cast<int>(lanes.size()));

    for (int row = 0; row < static_cast<int>(lanes.size()); ++row) {
        auto const & lane = lanes[static_cast<size_t>(row)];

        auto setCell = [this, row](int col, QString const & text) {
            auto * item = _lane_table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                _lane_table->setItem(row, col, item);
            }
            item->setText(text);
        };

        setCell(0, QString::number(lane.lane_index));
        setCell(1, QString::fromStdString(lane.series_key));
        setCell(2, QString::fromStdString(lane.series_type));
        setCell(3, lane.is_visible ? QStringLiteral("Y") : QStringLiteral("N"));
        setCell(4, QString::number(static_cast<double>(lane.center_y_ndc), 'f', 4));
        setCell(5, QString::number(static_cast<double>(lane.height_ndc), 'f', 4));
        setCell(6, QString::number(static_cast<double>(lane.transform_offset), 'f', 4));
        setCell(7, QString::number(static_cast<double>(lane.transform_gain), 'f', 4));
        setCell(8, QString::number(static_cast<double>(lane.user_scale_factor), 'f', 2));
        setCell(9, QString::number(static_cast<double>(lane.data_mean), 'f', 4));
        setCell(10, QString::number(static_cast<double>(lane.data_std_dev), 'f', 4));
        setCell(11, lane.is_on_screen ? QStringLiteral("Y") : QStringLiteral("N"));
    }

    _lane_table->resizeColumnsToContents();
}

void DataViewerDebugPanel::_refreshRenderingStats() {
    if (!_opengl_widget) {
        return;
    }

    auto const diag = _opengl_widget->getDiagnostics();
    auto const & r = diag.rendering;

    _batch_counts_label->setText(
            QStringLiteral("Batches: poly=%1  glyph=%2  rect=%3")
                    .arg(r.polyline_batch_count)
                    .arg(r.glyph_batch_count)
                    .arg(r.rectangle_batch_count));

    _vertex_count_label->setText(
            QStringLiteral("Total vertices: %1").arg(r.total_vertex_count));

    _entity_count_label->setText(
            QStringLiteral("Entities: %1").arg(r.total_entity_count));

    _rebuild_count_label->setText(
            QStringLiteral("Scene rebuilds: %1").arg(r.scene_rebuild_count));
}
