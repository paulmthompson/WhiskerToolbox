#include "PSTHPlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

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
    // Skip rebuild if we're updating y_max from rebuildScene to prevent loop
    if (_updating_y_max_from_rebuild) {
        return;
    }
    _scene_dirty = true;
    emit viewBoundsChanged();
    update();
}

void PSTHPlotOpenGLWidget::rebuildScene()
{
    if (!_state || !_data_manager) {
        return;
    }

    // Get plot events from state
    auto event_names = _state->getPlotEventNames();
    if (event_names.empty()) {
        qDebug() << "PSTHPlotOpenGLWidget: No plot events configured";
        return;
    }

    // Get alignment state
    auto alignment_state = _state->alignmentState();
    if (!alignment_state) {
        qDebug() << "PSTHPlotOpenGLWidget: No alignment state";
        return;
    }

    // Get window size and bin size from state
    double window_size = _state->getWindowSize();
    double bin_size = _state->getBinSize();
    double half_window = window_size / 2.0;

    // Calculate number of bins: from -window/2 to +window/2 with bin_size spacing
    int num_bins = static_cast<int>(std::ceil(window_size / bin_size));
    if (num_bins <= 0) {
        qDebug() << "PSTHPlotOpenGLWidget: Invalid bin configuration";
        return;
    }

    // Initialize histogram bins (counts per bin)
    std::vector<double> histogram(num_bins, 0.0);
    size_t total_trials = 0;

    // Process each plot event series
    for (auto const & event_name : event_names) {
        auto const event_options = _state->getPlotEventOptions(event_name);
        if (!event_options || event_options->event_key.empty()) {
            continue;
        }

        // Gather aligned event data using PlotAlignmentGather
        auto gathered = WhiskerToolbox::Plots::createAlignedGatherResult<DigitalEventSeries>(
            _data_manager,
            event_options->event_key,
            alignment_state->data());

        if (gathered.empty()) {
            qDebug() << "PSTHPlotOpenGLWidget: Empty gather result for event" << event_name;
            continue;
        }

        // Get the source series to access its TimeFrame
        auto source_series = _data_manager->getData<DigitalEventSeries>(event_options->event_key);
        if (!source_series) {
            qDebug() << "PSTHPlotOpenGLWidget: Could not get source series";
            continue;
        }

        auto time_frame = source_series->getTimeFrame();
        if (!time_frame) {
            qDebug() << "PSTHPlotOpenGLWidget: Source series has no TimeFrame";
            continue;
        }

        // Process each trial in the gather result
        for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
            auto const & trial_view = gathered[trial_idx];
            if (!trial_view) {
                continue;
            }

            // Get the alignment time for this trial (center point)
            // The alignment time is the reference point (t=0) for relative time calculation
            int64_t alignment_time = gathered.alignmentTimeAt(trial_idx);
            int alignment_time_abs = time_frame->getTimeAtIndex(TimeFrameIndex{alignment_time});

            // Iterate over events in this trial view
            for (auto const & event_with_id : trial_view->view()) {
                // Get event time as TimeFrameIndex
                TimeFrameIndex event_time_idx = event_with_id.event_time;

                // Convert event time index to absolute time
                int event_time_abs = time_frame->getTimeAtIndex(event_time_idx);

                // Calculate relative time (normalized by alignment event)
                // This gives us the time relative to the alignment point (t=0)
                double relative_time = static_cast<double>(event_time_abs - alignment_time_abs);

                // Only include events within the window
                if (relative_time < -half_window || relative_time >= half_window) {
                    continue;
                }

                // Calculate which bin this event belongs to
                // Bin 0 corresponds to -half_window, bin (num_bins-1) corresponds to +half_window
                double bin_position = (relative_time + half_window) / bin_size;
                int bin_index = static_cast<int>(std::floor(bin_position));

                // Clamp bin index to valid range
                bin_index = std::max(0, std::min(bin_index, num_bins - 1));

                // Increment the count for this bin
                histogram[bin_index] += 1.0;
            }
            
            // Track total number of trials processed
            total_trials++;
        }
    }

    // Print histogram construction
    qDebug() << "PSTHPlotOpenGLWidget: Histogram constructed";
    qDebug() << "  Window size:" << window_size;
    qDebug() << "  Bin size:" << bin_size;
    qDebug() << "  Number of bins:" << num_bins;
    qDebug() << "  Bin range: [" << -half_window << ":" << bin_size << ":" << half_window << "]";
    
    // Print some statistics
    double total_events = 0.0;
    double max_count = 0.0;
    for (double count : histogram) {
        total_events += count;
        max_count = std::max(max_count, count);
    }
    qDebug() << "  Total events:" << total_events;
    qDebug() << "  Max bin count:" << max_count;
    qDebug() << "  Number of trials:" << total_trials;

    // Update y_max to match the maximum histogram value (with some padding)
    // Use a flag to prevent rebuild loop
    if (_state && max_count > 0.0) {
        // Add 10% padding above the maximum
        double new_y_max = max_count * 1.1;
        // Only update if the value actually changed (to avoid unnecessary signals)
        if (std::abs(_state->getYMax() - new_y_max) > 0.01) {
            _updating_y_max_from_rebuild = true;
            _state->setYMax(new_y_max);
            _updating_y_max_from_rebuild = false;
        }
    }

    // TODO: Store histogram for rendering later
    // _histogram = histogram;
    // _histogram_bin_size = bin_size;
    // _histogram_window_size = window_size;
}

QPointF PSTHPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    // TODO: Implement coordinate transformation
    // For now, return a default value
    return QPointF(0.0, 0.0);
}
