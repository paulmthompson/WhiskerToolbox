#ifndef HEATMAP_OPENGLWIDGET_HPP
#define HEATMAP_OPENGLWIDGET_HPP

/**
 * @file HeatmapOpenGLWidget.hpp
 * @brief OpenGL-based heatmap visualization using CorePlotting infrastructure
 *
 * Uses CorePlotting::ViewStateData for view state and WhiskerToolbox::Plots
 * helpers for projection and interaction.
 */

#include "Core/HeatmapState.hpp"

#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/Mappers/HeatmapMapper.hpp"
#include "Plots/Common/TooltipManager/PlotTooltipManager.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "TimeFrame/TimeFrame.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QString>

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class QAction;
class QContextMenuEvent;
class QMenu;
class QMouseEvent;
class QWheelEvent;
class SelectionContext;

class HeatmapOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit HeatmapOpenGLWidget(QWidget * parent = nullptr);
    ~HeatmapOpenGLWidget() override;

    HeatmapOpenGLWidget(HeatmapOpenGLWidget const &) = delete;
    HeatmapOpenGLWidget & operator=(HeatmapOpenGLWidget const &) = delete;
    HeatmapOpenGLWidget(HeatmapOpenGLWidget &&) = delete;
    HeatmapOpenGLWidget & operator=(HeatmapOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<HeatmapState> state);
    void setDataManager(std::shared_ptr<DataManager> data_manager);
    /** @brief Current view state (from state; for axis getters) */
    [[nodiscard]] CorePlotting::ViewStateData const & viewState() const;
    void resetView();

    /**
     * @brief Set the SelectionContext for ContextAction integration in the context menu
     * @param selection_context Pointer to the SelectionContext (not owned)
     */
    void setSelectionContext(SelectionContext * selection_context);

    /**
     * @brief Serialize the current cached scene to SVG (widget pixel size, current camera).
     * @return UTF-8 SVG document, or empty if there is no rectangle geometry to export.
     */
    [[nodiscard]] QString exportToSVG();

    /**
     * @brief Collect aggregate heatmap export data by re-running the pipeline.
     *
     * Re-executes the heatmap data pipeline with the current state settings and
     * returns the sorted rows and unit keys ready for `exportHeatmapToCSV()`.
     */
    struct HeatmapExportBundle {
        std::vector<std::string> unit_keys;
        std::vector<CorePlotting::HeatmapRowData> rows;
    };
    [[nodiscard]] HeatmapExportBundle collectHeatmapExportData() const;

signals:
    void plotDoubleClicked(int64_t time_frame_index);
    void viewBoundsChanged();
    void unitCountChanged(size_t count);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void showEvent(QShowEvent * event) override;

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
    void leaveEvent(QEvent * event) override;
    void contextMenuEvent(QContextMenuEvent * event) override;

private slots:
    void onStateChanged();
    void onViewStateChanged();

private:
    std::shared_ptr<HeatmapState> _state;
    std::shared_ptr<DataManager> _data_manager;

    PlottingOpenGL::SceneRenderer _scene_renderer;
    /** @brief Cached scene (same geometry as GPU); reserved for SVG export */
    CorePlotting::RenderableScene _scene;
    bool _opengl_initialized{false};
    bool _scene_dirty{true};

    /** @brief Cached view state (single source of truth is HeatmapState) */
    CorePlotting::ViewStateData _cached_view_state;
    glm::mat4 _view_matrix{1.0f};
    glm::mat4 _projection_matrix{1.0f};

    bool _is_panning{false};
    QPoint _last_mouse_pos;
    QPoint _click_start_pos;
    static constexpr int DRAG_THRESHOLD = 5;

    size_t _unit_count{0};

    /// Cached display-order unit keys (reflects current sort order)
    std::vector<std::string> _display_unit_keys;

    std::unique_ptr<WhiskerToolbox::Plots::PlotTooltipManager> _tooltip_mgr;

    SelectionContext * _selection_context{nullptr};
    QMenu * _context_menu{nullptr};
    std::vector<QAction *> _dynamic_context_actions;

    int _widget_width{1};
    int _widget_height{1};

    void rebuildScene();
    void setupTooltip();
    [[nodiscard]] int worldToUnitIndex(QPointF const & world_pos) const;
    void updateMatrices();
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes);
    void updateBackgroundColor();
};

#endif// HEATMAP_OPENGLWIDGET_HPP
