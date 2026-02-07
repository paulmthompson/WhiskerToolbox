#ifndef HEATMAP_OPENGLWIDGET_HPP
#define HEATMAP_OPENGLWIDGET_HPP

#include "Core/HeatmapState.hpp"

#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DataManager/utils/GatherResult.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>

class DataManager;
class QMouseEvent;
class QWheelEvent;

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
    [[nodiscard]] HeatmapViewState const & viewState() const;
    void resetView();

signals:
    void plotDoubleClicked(int64_t time_frame_index);
    void viewBoundsChanged();
    void trialCountChanged(size_t count);

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
    std::shared_ptr<HeatmapState> _state;
    std::shared_ptr<DataManager> _data_manager;

    PlottingOpenGL::SceneRenderer _scene_renderer;
    bool _opengl_initialized{false};
    bool _scene_dirty{true};

    HeatmapViewState _cached_view_state;
    glm::mat4 _view_matrix{1.0f};
    glm::mat4 _projection_matrix{1.0f};

    // Panning state
    bool _is_panning{false};
    QPoint _last_mouse_pos;
    QPoint _click_start_pos;
    static constexpr int DRAG_THRESHOLD = 5;

    size_t _trial_count{0};

    int _widget_width{1};
    int _widget_height{1};

    void rebuildScene();
    void updateMatrices();
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes = false);
    void updateBackgroundColor();
};

#endif // HEATMAP_OPENGLWIDGET_HPP
