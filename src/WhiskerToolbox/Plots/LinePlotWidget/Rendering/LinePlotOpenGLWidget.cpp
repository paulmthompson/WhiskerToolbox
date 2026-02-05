#include "LinePlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CoreGeometry/boundingbox.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QString>
#include <QWheelEvent>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

LinePlotOpenGLWidget::LinePlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent)
{
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Request OpenGL 4.1 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling for smooth lines
    setFormat(format);
}

LinePlotOpenGLWidget::~LinePlotOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void LinePlotOpenGLWidget::setState(std::shared_ptr<LinePlotState> state)
{
    // Disconnect old state signals
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        // Connect to state signals
        connect(_state.get(), &LinePlotState::stateChanged,
                this, &LinePlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &LinePlotState::windowSizeChanged,
                this, &LinePlotOpenGLWidget::onWindowSizeChanged);
        connect(_state.get(), &LinePlotState::plotSeriesAdded,
                this, [this](QString const & /* series_name */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &LinePlotState::plotSeriesRemoved,
                this, [this](QString const & /* series_name */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &LinePlotState::plotSeriesOptionsChanged,
                this, [this](QString const & /* series_name */) {
                    _scene_dirty = true;
                    update();
                });

        // Initial sync
        _scene_dirty = true;
    }
}

void LinePlotOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

std::pair<double, double> LinePlotOpenGLWidget::getViewBounds() const
{
    if (!_state) {
        return {-500.0, 500.0};  // Default bounds
    }

    double window_size = _state->getWindowSize();
    double half_window = window_size / 2.0;
    return {-half_window, half_window};
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void LinePlotOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Set clear color (dark theme)
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable blending for smooth lines
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable multisampling if available
    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize the scene renderer
    if (!_scene_renderer.initialize()) {
        qWarning() << "LinePlotOpenGLWidget: Failed to initialize SceneRenderer";
        return;
    }

    _opengl_initialized = true;
    updateMatrices();
}

void LinePlotOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_opengl_initialized) {
        return;
    }

    // Rebuild scene if needed
    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    // Render the scene
    _scene_renderer.render(_view_matrix, _projection_matrix);
}

void LinePlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);

    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void LinePlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = true;
        _last_mouse_pos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    event->accept();
}

void LinePlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    if (_is_panning) {
        int delta_x = event->pos().x() - _last_mouse_pos.x();
        int delta_y = event->pos().y() - _last_mouse_pos.y();
        handlePanning(delta_x, delta_y);
        _last_mouse_pos = event->pos();
    }

    event->accept();
}

void LinePlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    event->accept();
}

void LinePlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF world = screenToWorld(event->pos());
        // TODO: Emit plotDoubleClicked with proper time frame index
        emit plotDoubleClicked(static_cast<int64_t>(world.x()));
    }
    event->accept();
}

void LinePlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float delta = event->angleDelta().y() / 120.0f;
    bool y_only = event->modifiers() & Qt::ShiftModifier;
    handleZoom(delta, y_only);
    event->accept();
}

// =============================================================================
// Slots
// =============================================================================

void LinePlotOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void LinePlotOpenGLWidget::onWindowSizeChanged(double /* window_size */)
{
    _scene_dirty = true;
    updateMatrices();
    emit viewBoundsChanged();
    update();
}

// =============================================================================
// Private Methods
// =============================================================================

void LinePlotOpenGLWidget::rebuildScene()
{
    
}

void LinePlotOpenGLWidget::updateMatrices()
{
    if (!_state) {
        return;
    }

    // Get window size for bounds
    double window_size = _state->getWindowSize();
    double half_window = window_size / 2.0;

    // Build orthographic projection
    _projection_matrix = glm::ortho(
        static_cast<float>(-half_window),
        static_cast<float>(half_window),
        -1.0f,
        1.0f,
        -1.0f,
        1.0f
    );
    _view_matrix = glm::mat4(1.0f);
}

QPointF LinePlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    // Normalize screen coordinates to [-1, 1]
    float ndc_x = (2.0f * screen_pos.x() / _widget_width) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_pos.y() / _widget_height); // Flip Y

    // Invert projection to get world coordinates
    glm::mat4 inv_proj = glm::inverse(_projection_matrix);
    glm::vec4 ndc(ndc_x, ndc_y, 0.0f, 1.0f);
    glm::vec4 world = inv_proj * ndc;

    return QPointF(world.x, world.y);
}

void LinePlotOpenGLWidget::handlePanning(int /* delta_x */, int /* delta_y */)
{
    // TODO: Implement panning
    // For now, panning is not implemented
}

void LinePlotOpenGLWidget::handleZoom(float /* delta */, bool /* y_only */)
{
    // TODO: Implement zooming
    // For now, zooming is not implemented
}

GatherResult<AnalogTimeSeries> LinePlotOpenGLWidget::gatherTrialData() const
{
    if (!_data_manager || !_state) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Get the first series key from the plot series
    auto series_names = _state->getPlotSeriesNames();
    if (series_names.empty()) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Get the first series's options
    auto const series_options = _state->getPlotSeriesOptions(series_names.front());
    if (!series_options || series_options->series_key.empty()) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Get alignment state
    auto alignment_state = _state->alignmentState();
    if (!alignment_state) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Use the PlotAlignmentGather API for AnalogTimeSeries
    return WhiskerToolbox::Plots::createAlignedGatherResult<AnalogTimeSeries>(
        _data_manager,
        series_options->series_key,
        alignment_state->data());
}
