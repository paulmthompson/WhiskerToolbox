#ifndef PSTHPLOT_OPENGLWIDGET_HPP
#define PSTHPLOT_OPENGLWIDGET_HPP

/**
 * @file PSTHPlotOpenGLWidget.hpp
 * @brief OpenGL-based PSTH plot visualization widget
 * 
 * This widget renders Peri-Stimulus Time Histogram plots showing
 * event counts or rates aligned to a reference event.
 * 
 * Architecture:
 * - Receives PSTHState for alignment, bin size, and plot options
 * - Uses GatherResult<DigitalEventSeries> for trial-aligned data
 * - Uses PlottingOpenGL::SceneRenderer for OpenGL rendering
 * 
 * @see PlottingOpenGL/SceneRenderer.hpp
 */

#include "Core/PSTHState.hpp"

#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/DataTypes/HistogramData.hpp"
#include "CorePlotting/Mappers/HistogramMapper.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DataManager/utils/GatherResult.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>

class DataManager;
class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering PSTH plots
 * 
 * Displays histogram of event counts aligned to trial intervals.
 * Supports pan/zoom interaction on both axes.
 */
class PSTHPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PSTHPlotOpenGLWidget(QWidget * parent = nullptr);
    ~PSTHPlotOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    PSTHPlotOpenGLWidget(PSTHPlotOpenGLWidget const &) = delete;
    PSTHPlotOpenGLWidget & operator=(PSTHPlotOpenGLWidget const &) = delete;
    PSTHPlotOpenGLWidget(PSTHPlotOpenGLWidget &&) = delete;
    PSTHPlotOpenGLWidget & operator=(PSTHPlotOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<PSTHState> state);
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    [[nodiscard]] std::pair<double, double> getViewBounds() const;

signals:
    void plotDoubleClicked(int64_t time_frame_index);
    void viewBoundsChanged();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private slots:
    void onStateChanged();
    void onViewStateChanged();

private:
    std::shared_ptr<PSTHState> _state;
    std::shared_ptr<DataManager> _data_manager;

    // Rendering
    PlottingOpenGL::SceneRenderer _scene_renderer;
    bool _opengl_initialized{false};
    bool _scene_dirty{true};

    // Viewport
    int _widget_width{1};
    int _widget_height{1};

    // Pan/zoom interaction
    bool _is_panning{false};
    QPoint _click_start_pos;
    QPoint _last_mouse_pos;
    static constexpr int DRAG_THRESHOLD = 4;

    // View state cache (single source of truth is PSTHState)
    CorePlotting::ViewStateData _cached_view_state;
    glm::mat4 _projection_matrix{1.0f};
    glm::mat4 _view_matrix{1.0f};

    // Flag to prevent rebuild loop when updating y_max from rebuildScene
    bool _updating_y_max_from_rebuild{false};

    void rebuildScene();
    void uploadHistogramScene();
    void updateMatrices();
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes);
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;

    // Cached histogram data from last rebuildScene()
    CorePlotting::HistogramData _histogram_data;
    CorePlotting::HistogramStyle _histogram_style;
};

#endif // PSTHPLOT_OPENGLWIDGET_HPP
