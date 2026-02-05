#include "PSTHPlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <glm/gtc/matrix_transform.hpp>

PSTHPlotOpenGLWidget::PSTHPlotOpenGLWidget(QWidget * parent)
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
    format.setSamples(4); // Enable multisampling
    setFormat(format);
}

PSTHPlotOpenGLWidget::~PSTHPlotOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void PSTHPlotOpenGLWidget::setState(std::shared_ptr<PSTHState> state)
{
    // Disconnect old state signals
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        // Connect to state signals
        connect(_state.get(), &PSTHState::stateChanged,
                this, &PSTHPlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &PSTHState::windowSizeChanged,
                this, [this](double /* window_size */) {
                    _scene_dirty = true;
                    emit viewBoundsChanged();
                    update();
                });

        // Initial sync
        _scene_dirty = true;
    }
}

void PSTHPlotOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

std::pair<double, double> PSTHPlotOpenGLWidget::getViewBounds() const
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

void PSTHPlotOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Set clear color (dark theme)
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable multisampling if available
    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize the scene renderer
    if (!_scene_renderer.initialize()) {
        qWarning() << "PSTHPlotOpenGLWidget: Failed to initialize SceneRenderer";
        return;
    }

    _opengl_initialized = true;
}

void PSTHPlotOpenGLWidget::paintGL()
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

    // TODO: Render histogram bars/lines here
    // For now, just clear the screen
}

void PSTHPlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = w;
    _widget_height = h;
    glViewport(0, 0, w, h);
    _scene_dirty = true;
    update();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void PSTHPlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    QOpenGLWidget::mousePressEvent(event);
}

void PSTHPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseMoveEvent(event);
}

void PSTHPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseReleaseEvent(event);
}

void PSTHPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        auto world_pos = screenToWorld(event->pos());
        // TODO: Convert world position to time frame index
        // For now, emit with 0
        emit plotDoubleClicked(0);
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void PSTHPlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    QOpenGLWidget::wheelEvent(event);
}

// =============================================================================
// Private Methods
// =============================================================================

void PSTHPlotOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    emit viewBoundsChanged();
    update();
}

void PSTHPlotOpenGLWidget::rebuildScene()
{
    if (!_state || !_data_manager) {
        return;
    }

    // TODO: Implement PSTH histogram building
    // 1. Gather aligned event data using PlotAlignmentGather
    // 2. Bin events by time
    // 3. Create renderable batches (bars or lines)
    // 4. Update scene
}

QPointF PSTHPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    // TODO: Implement coordinate transformation
    // For now, return a default value
    return QPointF(0.0, 0.0);
}
