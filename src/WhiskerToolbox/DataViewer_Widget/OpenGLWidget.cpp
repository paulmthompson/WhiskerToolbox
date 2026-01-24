#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/CoordinateTransform/SeriesCoordinateQuery.hpp"
#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "DataManager/utils/color.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DataViewerCoordinates.hpp"
#include "DataViewerSelectionManager.hpp"
#include "DataViewerTooltipController.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"
#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"
#include "SceneBuildingHelpers.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeSeriesDataStore.hpp"
#include "TransformComposers.hpp"

#include <QEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QToolTip>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <ranges>


OpenGLWidget::OpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent)
    , _state(std::make_shared<DataViewerState>()) {
    setMouseTracking(true);// Enable mouse tracking for hover events

    // Connect state signals
    connect(_state.get(), &DataViewerState::viewStateChanged, this, [this]() {
        _cache_state.layout_response_dirty = true;
        _cache_state.scene_dirty = true;
        update();
    });
    connect(_state.get(), &DataViewerState::themeChanged, this, [this]() {
        update();
    });
    connect(_state.get(), &DataViewerState::gridChanged, this, [this]() {
        update();
    });

    // Initialize data store and connect to state's series options registry
    _data_store = std::make_unique<DataViewer::TimeSeriesDataStore>(this);
    _data_store->setSeriesOptionsRegistry(&_state->seriesOptions());
    connect(_data_store.get(), &DataViewer::TimeSeriesDataStore::layoutDirty, this, [this]() {
        _cache_state.layout_response_dirty = true;
        updateCanvas(_time);
    });

    // Initialize selection manager
    _selection_manager = std::make_unique<DataViewer::DataViewerSelectionManager>(this);
    connect(_selection_manager.get(), &DataViewer::DataViewerSelectionManager::selectionChanged, this, [this](EntityId id, bool selected) {
        emit entitySelectionChanged(id, selected);
    });
    connect(_selection_manager.get(), &DataViewer::DataViewerSelectionManager::selectionModified, this, [this]() {
        _cache_state.scene_dirty = true;
        updateCanvas(_time);
    });

    // Initialize tooltip controller
    _tooltip_controller = std::make_unique<DataViewer::DataViewerTooltipController>(this);
    _tooltip_controller->setSeriesInfoProvider([this](float canvas_x, float canvas_y) -> std::optional<DataViewer::SeriesInfo> {
        auto result = findSeriesAtPosition(canvas_x, canvas_y);
        if (!result.has_value()) {
            return std::nullopt;
        }
        DataViewer::SeriesInfo info;
        info.type = result->first;
        info.key = result->second;

        // Get analog value if applicable
        if (info.type == "Analog") {
            info.value = canvasYToAnalogValue(canvas_y, info.key);
            info.has_value = true;
        }
        return info;
    });

    // Initialize input handler
    _input_handler = std::make_unique<DataViewer::DataViewerInputHandler>(this);

    // Connect input handler signals
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::panDelta, this, [this](float normalized_dy) {
        // Apply pan delta directly to the state's view state
        auto view = _state->viewState();
        view.applyVerticalPanDelta(normalized_dy);
        _state->setViewState(view);
        update();
    });
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::clicked, this, &OpenGLWidget::mouseClick);
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::hoverCoordinates, this, &OpenGLWidget::mouseHover);
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::entityClicked, this, [this](EntityId id, bool ctrl_pressed) {
        // Delegate to selection manager
        _selection_manager->handleEntityClick(id, ctrl_pressed);
    });
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::intervalEdgeDragRequested, this, [this](CorePlotting::HitTestResult const & hit_result) {
        // Get series color for preview from state
        auto const * opts = _state->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(hit_result.series_key));
        if (opts) {
            int r, g, b;
            hexToRGB(opts->hex_color(), r, g, b);
            glm::vec4 fill_color(r / 255.0f, g / 255.0f, b / 255.0f, 0.5f);
            glm::vec4 stroke_color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
            _interaction_manager->startEdgeDrag(hit_result, fill_color, stroke_color);
        }
    });
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::intervalCreationRequested, this, [this](QString const &, QPoint const & start_pos) {
        // Find first visible digital interval series from state
        for (auto const & [series_key, data]: _data_store->intervalSeries()) {
            auto const * opts = _state->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(series_key));
            if (opts && opts->get_is_visible()) {
                int r, g, b;
                hexToRGB(opts->hex_color(), r, g, b);
                glm::vec4 fill_color(r / 255.0f, g / 255.0f, b / 255.0f, 0.5f);
                glm::vec4 stroke_color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
                _interaction_manager->startIntervalCreation(
                        series_key,
                        static_cast<float>(start_pos.x()),
                        static_cast<float>(start_pos.y()),
                        fill_color, stroke_color);
                _input_handler->setInteractionActive(true);
                return;
            }
        }
    });
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::cursorChangeRequested, this, [this](Qt::CursorShape cursor) {
        setCursor(cursor);
    });
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::tooltipRequested, this, [this](QPoint const & pos) {
        _tooltip_controller->scheduleTooltip(pos);
    });
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::tooltipCancelled, this, [this]() {
        _tooltip_controller->cancel();
    });
    connect(_input_handler.get(), &DataViewer::DataViewerInputHandler::repaintRequested, this, [this]() {
        update();
    });

    // Set up series info callback for input handler
    _input_handler->setSeriesInfoCallback([this](float canvas_x, float canvas_y) {
        return findSeriesAtPosition(canvas_x, canvas_y);
    });
    _input_handler->setAnalogValueCallback([this](float canvas_y, std::string const & key) {
        return canvasYToAnalogValue(canvas_y, key);
    });

    // Initialize interaction manager
    _interaction_manager = std::make_unique<DataViewer::DataViewerInteractionManager>(this);

    // Connect interaction manager signals
    connect(_interaction_manager.get(), &DataViewer::DataViewerInteractionManager::modeChanged, this, [this](DataViewer::InteractionMode mode) {
        emit interactionModeChanged(mode);
    });
    connect(_interaction_manager.get(), &DataViewer::DataViewerInteractionManager::interactionCompleted, this, [this](CorePlotting::Interaction::DataCoordinates const & coords) {
        handleInteractionCompleted(coords);
        _input_handler->setInteractionActive(false);
    });
    connect(_interaction_manager.get(), &DataViewer::DataViewerInteractionManager::previewUpdated, this, [this]() {
        updateCanvas(_time);
    });
    connect(_interaction_manager.get(), &DataViewer::DataViewerInteractionManager::cursorChangeRequested, this, [this](Qt::CursorShape cursor) {
        setCursor(cursor);
    });
}

OpenGLWidget::~OpenGLWidget() {
    // Disconnect the context destruction signal BEFORE cleanup to prevent lambda from accessing destroyed object
    if (_gl_state.ctx_about_to_be_destroyed_conn) {
        disconnect(_gl_state.ctx_about_to_be_destroyed_conn);
        _gl_state.ctx_about_to_be_destroyed_conn = QMetaObject::Connection();
    }
    cleanup();
}

void OpenGLWidget::updateCanvas(TimeFrameIndex time) {
    _time = time;

    // Update view state immediately so labels are correct before paintGL runs
    // Use the underlying TimeSeriesViewState's setTimeWindow(center, width) method
    auto view = _state->viewState();
    int64_t const width = view.getTimeWidth();
    view.setTimeWindow(_time.getValue(), width);
    _state->setViewState(view);

    // Mark layout and scene as dirty since external changes may have occurred
    // (e.g., display_mode changes, series visibility changes)
    _cache_state.layout_response_dirty = true;
    _cache_state.scene_dirty = true;
    //std::cout << "Redrawing at " << _time << std::endl;
    update();
}

// Mouse event handlers - delegate to input handler and interaction manager
void OpenGLWidget::mousePressEvent(QMouseEvent * event) {
    // Update input handler context before processing
    DataViewer::InputContext ctx;
    ctx.view_state = &_state->viewState();
    ctx.layout_response = &_cache_state.layout_response;
    ctx.scene = &_cache_state.scene;
    ctx.selected_entities = &_selection_manager->selectedEntities();
    ctx.rectangle_batch_key_map = &_cache_state.rectangle_batch_key_map;
    ctx.widget_width = width();
    ctx.widget_height = height();
    _input_handler->setContext(ctx);

    if (_input_handler->handleMousePress(event)) {
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    // Check if interaction manager is handling this
    if (_interaction_manager->isActive()) {
        float const canvas_x = static_cast<float>(event->pos().x());
        float const canvas_y = static_cast<float>(event->pos().y());
        _interaction_manager->update(canvas_x, canvas_y);
        _tooltip_controller->cancel();
        return;
    }

    // Update input handler context
    DataViewer::InputContext ctx;
    ctx.view_state = &_state->viewState();
    ctx.layout_response = &_cache_state.layout_response;
    ctx.scene = &_cache_state.scene;
    ctx.selected_entities = &_selection_manager->selectedEntities();
    ctx.rectangle_batch_key_map = &_cache_state.rectangle_batch_key_map;
    ctx.widget_width = width();
    ctx.widget_height = height();
    _input_handler->setContext(ctx);

    _input_handler->handleMouseMove(event);
    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        // Check if interaction manager is handling this
        if (_interaction_manager->isActive()) {
            _interaction_manager->complete();
            return;
        }

        _input_handler->handleMouseRelease(event);
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLWidget::leaveEvent(QEvent * event) {
    _input_handler->handleLeave();
    QOpenGLWidget::leaveEvent(event);
}

void OpenGLWidget::setState(std::shared_ptr<DataViewerState> state) {
    if (_state) {
        // Disconnect old state signals
        disconnect(_state.get(), nullptr, this, nullptr);
    }
    
    _state = std::move(state);
    
    if (_state) {
        // Connect state signals - state changes trigger redraws
        connect(_state.get(), &DataViewerState::viewStateChanged, this, [this]() {
            _cache_state.layout_response_dirty = true;
            _cache_state.scene_dirty = true;
            update();
        });
        connect(_state.get(), &DataViewerState::themeChanged, this, [this]() {
            update();
        });
        connect(_state.get(), &DataViewerState::gridChanged, this, [this]() {
            update();
        });
        
        // Update data store's reference to series options registry
        if (_data_store) {
            _data_store->setSeriesOptionsRegistry(&_state->seriesOptions());
        }
    }
    
    updateCanvas(_time);
}

CorePlotting::TimeSeriesViewState const & OpenGLWidget::getViewState() const {
    return _state->viewState();
}

std::string OpenGLWidget::getBackgroundColor() const {
    return _state->backgroundColor().toStdString();
}

void OpenGLWidget::cleanup() {
    // Avoid re-entrancy or cleanup without a valid context
    if (!_gl_state.initialized) {
        return;
    }

    // Guard: QOpenGLContext may already be gone during teardown
    if (QOpenGLContext::currentContext() == nullptr && context() == nullptr) {
        _gl_state.initialized = false;
        return;
    }

    // Safe to release our GL resources
    makeCurrent();

    // Cleanup PlottingOpenGL SceneRenderer
    if (_scene_renderer) {
        _scene_renderer->cleanup();
        _scene_renderer.reset();
    }

    // Cleanup AxisRenderer
    if (_axis_renderer) {
        _axis_renderer->cleanup();
        _axis_renderer.reset();
    }

    doneCurrent();

    _gl_state.initialized = false;
}

void OpenGLWidget::initializeGL() {
    // Ensure QOpenGLFunctions is initialized
    initializeOpenGLFunctions();

    // Track GL init and connect context destruction
    _gl_state.initialized = true;
    if (context()) {
        // Disconnect any previous connection to avoid duplicates
        if (_gl_state.ctx_about_to_be_destroyed_conn) {
            disconnect(_gl_state.ctx_about_to_be_destroyed_conn);
        }
        // Use QPointer for safer object lifetime tracking
        QPointer<OpenGLWidget> self(this);
        _gl_state.ctx_about_to_be_destroyed_conn = connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [self]() {
            // Only call cleanup if the widget still exists
            if (self) {
                self->cleanup();
            }
        });
    }

    auto fmt = format();
    std::cout << "OpenGL major version: " << fmt.majorVersion() << std::endl;
    std::cout << "OpenGL minor version: " << fmt.minorVersion() << std::endl;
    int r, g, b;
    hexToRGB(_state->themeState().background_color, r, g, b);
    glClearColor(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Load axes shader (used by AxisRenderer)
    ShaderManager::instance().loadProgram(
            "axes",
            _gl_state.shader_source_type == ShaderSourceType::Resource ? ":/shaders/colored_vertex.vert" : "src/WhiskerToolbox/shaders/colored_vertex.vert",
            _gl_state.shader_source_type == ShaderSourceType::Resource ? ":/shaders/colored_vertex.frag" : "src/WhiskerToolbox/shaders/colored_vertex.frag",
            "",
            _gl_state.shader_source_type);
    // Load dashed line shader (used by AxisRenderer for grid)
    ShaderManager::instance().loadProgram(
            "dashed_line",
            _gl_state.shader_source_type == ShaderSourceType::Resource ? ":/shaders/dashed_line.vert" : "src/WhiskerToolbox/shaders/dashed_line.vert",
            _gl_state.shader_source_type == ShaderSourceType::Resource ? ":/shaders/dashed_line.frag" : "src/WhiskerToolbox/shaders/dashed_line.frag",
            "",
            _gl_state.shader_source_type);

    // Connect reload signal to redraw
    connect(&ShaderManager::instance(), &ShaderManager::shaderReloaded, this, [this](std::string const &) { update(); });

    // Initialize PlottingOpenGL SceneRenderer
    _scene_renderer = std::make_unique<PlottingOpenGL::SceneRenderer>();
    if (!_scene_renderer->initialize()) {
        std::cerr << "Warning: Failed to initialize SceneRenderer" << std::endl;
        _scene_renderer.reset();
    }

    // Initialize AxisRenderer
    _axis_renderer = std::make_unique<PlottingOpenGL::AxisRenderer>();
    if (!_axis_renderer->initialize()) {
        std::cerr << "Warning: Failed to initialize AxisRenderer" << std::endl;
        _axis_renderer.reset();
    }
}

///////////////////////////////////////////////////////////////////////////////
// All series rendering now uses SceneRenderer path via renderWithSceneRenderer()
// See SceneBuildingHelpers for batch construction and PlottingOpenGL for rendering
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::paintGL() {
    int r, g, b;
    hexToRGB(_state->themeState().background_color, r, g, b);
    glClearColor(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // View state is already updated in updateCanvas() - just use it here
    if (_scene_renderer && _scene_renderer->isInitialized()) {
        renderWithSceneRenderer();
    }

    drawAxis();

    drawGridLines();

    //unified controller preview overlay
    drawInteractionPreview();
}

void OpenGLWidget::resizeGL(int w, int h) {

    // Set the viewport to match the widget dimensions
    // This is crucial for proper scaling - it tells OpenGL the actual pixel dimensions
    glViewport(0, 0, w, h);

    // Store the new dimensions
    // Note: width() and height() will return the new values after this call

    // For 2D plotting, we should use orthographic projection
    _gl_state.proj.setToIdentity();
    _gl_state.proj.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);// Use orthographic projection for 2D plotting

    _gl_state.view.setToIdentity();
    _gl_state.view.translate(0, 0, -1);// Move slightly back for orthographic view

    // Trigger a repaint with the new dimensions
    update();
}

void OpenGLWidget::drawAxis() {
    if (!_axis_renderer || !_axis_renderer->isInitialized()) {
        return;
    }

    // Parse axis color from hex string
    auto const & view = _state->viewState();
    float r, g, b;
    hexToRGB(_state->themeState().axis_color, r, g, b);

    // Configure axis using AxisRenderer
    PlottingOpenGL::AxisConfig axis_config;
    axis_config.x_position = 0.0f;
    axis_config.y_min = view.y_min;
    axis_config.y_max = view.y_max;
    axis_config.color = glm::vec3(r, g, b);
    axis_config.alpha = 1.0f;

    // Convert QMatrix4x4 to glm::mat4
    glm::mat4 view_mat, proj_mat;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            view_mat[col][row] = _gl_state.view(row, col);
            proj_mat[col][row] = _gl_state.proj(row, col);
        }
    }

    _axis_renderer->renderAxis(axis_config, view_mat, proj_mat);
}

void OpenGLWidget::addAnalogTimeSeries(
        std::string const & key,
        std::shared_ptr<AnalogTimeSeries> series,
        std::string const & color) {
    // Add series data to data store (computes data_cache and layout_transform)
    _data_store->addAnalogSeries(key, std::move(series), color);
    
    // Create default display options in state if not already present
    QString const qkey = QString::fromStdString(key);
    if (!_state->seriesOptions().has<AnalogSeriesOptionsData>(qkey)) {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = color.empty() 
            ? DataViewer::DefaultColors::getColorForIndex(_data_store->analogSeries().size() - 1)
            : color;
        opts.is_visible() = true;
        
        // Configure gap detection based on data density (from the added series)
        auto const & analog_entry = _data_store->analogSeries().at(key);
        auto const & added_series = analog_entry.series;
        if (added_series->getTimeFrame()->getTotalFrameCount() / 5 > added_series->getNumSamples()) {
            opts.gap_handling = AnalogGapHandlingMode::AlwaysConnect;
        } else {
            opts.gap_handling = AnalogGapHandlingMode::DetectGaps;
            opts.enable_gap_detection = true;
            // Set gap threshold to 0.1% of total frames, with a minimum floor of 2
            float const calculated_threshold = static_cast<float>(added_series->getTimeFrame()->getTotalFrameCount()) / 1000.0f;
            opts.gap_threshold = std::max(2.0f, calculated_threshold);
        }
        
        _state->seriesOptions().set(qkey, opts);
    }
}

void OpenGLWidget::removeAnalogTimeSeries(std::string const & key) {
    _data_store->removeAnalogSeries(key);
    // Also remove options from state
    _state->seriesOptions().remove<AnalogSeriesOptionsData>(QString::fromStdString(key));
}

void OpenGLWidget::addDigitalEventSeries(
        std::string const & key,
        std::shared_ptr<DigitalEventSeries> series,
        std::string const & color) {
    // Add series data to data store
    _data_store->addEventSeries(key, std::move(series), color);
    
    // Create default display options in state if not already present
    QString const qkey = QString::fromStdString(key);
    if (!_state->seriesOptions().has<DigitalEventSeriesOptionsData>(qkey)) {
        DigitalEventSeriesOptionsData opts;
        opts.hex_color() = color.empty()
            ? DataViewer::DefaultColors::getColorForIndex(_data_store->eventSeries().size() - 1)
            : color;
        opts.is_visible() = true;
        _state->seriesOptions().set(qkey, opts);
    }
}

void OpenGLWidget::removeDigitalEventSeries(std::string const & key) {
    _data_store->removeEventSeries(key);
    // Also remove options from state
    _state->seriesOptions().remove<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
}

void OpenGLWidget::addDigitalIntervalSeries(
        std::string const & key,
        std::shared_ptr<DigitalIntervalSeries> series,
        std::string const & color) {
    // Add series data to data store
    _data_store->addIntervalSeries(key, std::move(series), color);
    
    // Create default display options in state if not already present
    QString const qkey = QString::fromStdString(key);
    if (!_state->seriesOptions().has<DigitalIntervalSeriesOptionsData>(qkey)) {
        DigitalIntervalSeriesOptionsData opts;
        opts.hex_color() = color.empty()
            ? DataViewer::DefaultColors::getColorForIndex(_data_store->intervalSeries().size() - 1)
            : color;
        opts.is_visible() = true;
        _state->seriesOptions().set(qkey, opts);
    }
}

void OpenGLWidget::removeDigitalIntervalSeries(std::string const & key) {
    _data_store->removeIntervalSeries(key);
    // Also remove options from state
    _state->seriesOptions().remove<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
}

void OpenGLWidget::clearSeries() {
    // Collect keys before clearing
    std::vector<std::string> analog_keys;
    std::vector<std::string> event_keys;
    std::vector<std::string> interval_keys;
    
    for (auto const& [key, _] : _data_store->analogSeries()) {
        analog_keys.push_back(key);
    }
    for (auto const& [key, _] : _data_store->eventSeries()) {
        event_keys.push_back(key);
    }
    for (auto const& [key, _] : _data_store->intervalSeries()) {
        interval_keys.push_back(key);
    }
    
    // Clear data store
    _data_store->clearAll();
    
    // Remove all options from state
    for (auto const& key : analog_keys) {
        _state->seriesOptions().remove<AnalogSeriesOptionsData>(QString::fromStdString(key));
    }
    for (auto const& key : event_keys) {
        _state->seriesOptions().remove<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
    }
    for (auto const& key : interval_keys) {
        _state->seriesOptions().remove<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
    }
}

void OpenGLWidget::drawGridLines() {
    auto const & grid = _state->gridState();
    if (!grid.enabled || !_axis_renderer || !_axis_renderer->isInitialized()) {
        return;// Grid lines are disabled or renderer not ready
    }

    // Configure grid
    auto const & view = _state->viewState();
    PlottingOpenGL::GridConfig grid_config;
    grid_config.time_start = view.time_start;
    grid_config.time_end = view.time_end;
    grid_config.spacing = grid.spacing;
    grid_config.y_min = view.y_min;
    grid_config.y_max = view.y_max;
    grid_config.color = glm::vec3(0.5f, 0.5f, 0.5f);// Gray grid lines
    grid_config.alpha = 0.5f;
    grid_config.dash_length = 3.0f;
    grid_config.gap_length = 3.0f;

    // Convert QMatrix4x4 to glm::mat4
    glm::mat4 view_mat, proj_mat;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            view_mat[col][row] = _gl_state.view(row, col);
            proj_mat[col][row] = _gl_state.proj(row, col);
        }
    }

    _axis_renderer->renderGrid(grid_config, view_mat, proj_mat, width(), height());
}

///////////////////////////////////////////////////////////////////////////////
// TimeRange / ViewState Methods
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::setMasterTimeFrame(std::shared_ptr<TimeFrame> master_time_frame) {
    _master_time_frame = master_time_frame;

    // Initialize time window based on TimeFrame
    if (master_time_frame) {
        int64_t const total_frames = master_time_frame->getTotalFrameCount();

        // Set a reasonable initial visible range (not the entire TimeFrame!)
        // Default to 10,000 samples or the full range if smaller
        constexpr int64_t DEFAULT_INITIAL_RANGE = 10000;
        int64_t const initial_range = std::min(DEFAULT_INITIAL_RANGE, total_frames);

        // Center at the beginning of the data
        int64_t const initial_center = initial_range / 2;
        auto view = _state->viewState();
        view.setTimeWindow(initial_center, initial_range);
        _state->setViewState(view);
    } else {
        // Reset to default time window
        _state->setViewState(CorePlotting::TimeSeriesViewState());
    }
}

///////////////////////////////////////////////////////////////////////////////

float OpenGLWidget::canvasXToTime(float canvas_x) const {
    // Use DataViewerCoordinates for coordinate transform
    DataViewer::DataViewerCoordinates const coords(_state->viewState(), width(), height());
    return coords.canvasXToTime(canvas_x);
}

float OpenGLWidget::canvasYToAnalogValue(float canvas_y, std::string const & series_key) const {
    // Find the series using data store
    auto const & analog_series = _data_store->analogSeries();
    auto const analog_it = analog_series.find(series_key);
    if (analog_it == analog_series.end()) {
        return 0.0f;// Series not found
    }

    // Get display options from state
    auto const * opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(series_key));
    if (!opts) {
        return 0.0f;
    }

    auto const & entry = analog_it->second;
    auto const & view = _state->viewState();

    // Step 1: Create DataViewerCoordinates for coordinate conversion
    DataViewer::DataViewerCoordinates const coords(view, width(), height());

    // Step 2: Get layout from cached response
    auto const * series_layout = _cache_state.layout_response.findLayout(series_key);
    CorePlotting::SeriesLayout layout = *series_layout;
    layout = *series_layout;

    // Step 3: Compose the same Y transform used for rendering
    // Data cache is stored in the entry, user scaling options in state
    CorePlotting::LayoutTransform y_transform = DataViewer::composeAnalogYTransform(
            layout,
            entry.data_cache.cached_mean,
            entry.data_cache.cached_std_dev,
            entry.data_cache.intrinsic_scale,
            opts->user_scale_factor,
            opts->y_offset,
            view.global_zoom,
            view.global_vertical_scale);

    // Step 4: Use DataViewerCoordinates to convert canvas Y to analog value
    return coords.canvasYToAnalogValue(canvas_y, y_transform);
}

// ========================================================================
// EntityId-based Selection API (delegates to SelectionManager)
// ========================================================================

void OpenGLWidget::selectEntity(EntityId id) {
    _selection_manager->select(id);
}

void OpenGLWidget::deselectEntity(EntityId id) {
    _selection_manager->deselect(id);
}

void OpenGLWidget::toggleEntitySelection(EntityId id) {
    _selection_manager->toggle(id);
}

void OpenGLWidget::clearEntitySelection() {
    _selection_manager->clear();
}

bool OpenGLWidget::isEntitySelected(EntityId id) const {
    return _selection_manager->isSelected(id);
}

std::unordered_set<EntityId> const & OpenGLWidget::getSelectedEntities() const {
    return _selection_manager->selectedEntities();
}


void OpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (_input_handler->handleDoubleClick(event)) {
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

///////////////////////////////////////////////////////////////////////////////
// Interaction Mode API - delegates to DataViewerInteractionManager
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::setInteractionMode(InteractionMode mode) {
    _interaction_manager->setMode(mode);
    _input_handler->setInteractionActive(_interaction_manager->isActive());
}

InteractionMode OpenGLWidget::interactionMode() const {
    return _interaction_manager->mode();
}

bool OpenGLWidget::isInteractionActive() const {
    return _interaction_manager->isActive();
}

void OpenGLWidget::cancelActiveInteraction() {
    _interaction_manager->cancel();
    _input_handler->setInteractionActive(false);
    updateCanvas(_time);
}

void OpenGLWidget::drawInteractionPreview() {
    auto preview = _interaction_manager->getPreview();
    if (!preview.has_value()) {
        return;
    }

    if (!_scene_renderer || !_scene_renderer->isInitialized()) {
        return;
    }

    _scene_renderer->previewRenderer().render(preview.value(), width(), height());
}

void OpenGLWidget::handleInteractionCompleted(CorePlotting::Interaction::DataCoordinates const & coords) {
    if (!coords.isInterval()) {
        // For now, only handle interval types
        return;
    }

    auto const & interval_coords = coords.asInterval();

    // Get the series data (access via data store)
    auto const & interval_series = _data_store->intervalSeries();
    auto it = interval_series.find(coords.series_key);
    if (it == interval_series.end()) {
        std::cout << "Series not found: " << coords.series_key << std::endl;
        return;
    }

    auto const & series = it->second.series;

    try {
        // The interval_coords are in master time frame (from view state)
        // Convert to series time frame if needed
        int64_t start_series, end_series;

        if (series->getTimeFrame().get() == _master_time_frame.get()) {
            // Same time frame - use coordinates directly
            start_series = interval_coords.start;
            end_series = interval_coords.end;
        } else {
            // Convert master time coordinates to series time frame indices
            start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(interval_coords.start)).getValue();
            end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(interval_coords.end)).getValue();
        }

        // Ensure proper ordering
        if (start_series > end_series) {
            std::swap(start_series, end_series);
        }

        // Validate converted coordinates
        if (start_series >= end_series || start_series < 0 || end_series < 0) {
            std::cout << "Invalid interval bounds after conversion" << std::endl;
            return;
        }

        if (coords.is_modification && coords.entity_id.has_value()) {
            // Modification mode: Find and modify existing interval by EntityId
            // Find the original interval by its EntityId
            auto original_interval = series->getIntervalByEntityId(*coords.entity_id);
            if (original_interval.has_value()) {
                // Remove the original interval
                for (int64_t time = original_interval->start; time <= original_interval->end; ++time) {
                    series->setEventAtTime(TimeFrameIndex(time), false);
                }

                std::cout << "Modified interval [" << original_interval->start << ", " << original_interval->end
                          << "] -> [" << start_series << ", " << end_series << "] for series "
                          << coords.series_key << std::endl;
            }
        }

        // Add the new/modified interval to the series
        series->addEvent(TimeFrameIndex(start_series), TimeFrameIndex(end_series));

        // Find and select the newly created/modified interval by its EntityId
        auto intervals = series->viewInRange(TimeFrameIndex(start_series),
                                             TimeFrameIndex(end_series),
                                             *series->getTimeFrame());

        for (auto const & interval_with_id: intervals) {
            if (interval_with_id.interval.start == start_series &&
                interval_with_id.interval.end == end_series) {
                clearEntitySelection();
                selectEntity(interval_with_id.entity_id);
                emit entitySelectionChanged(interval_with_id.entity_id, true);
                break;
            }
        }

        if (!coords.is_modification) {
            std::cout << "Created new interval [" << start_series
                      << ", " << end_series << "] for series "
                      << coords.series_key << std::endl;
        }

    } catch (std::exception const & e) {
        std::cout << "Error during interval operation: " << e.what() << std::endl;
    }

    // Trigger redraw
    updateCanvas(_time);
}

///////////////////////////////////////////////////////////////////////////////
// Series lookup for tooltips and input handling
///////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<std::string, std::string>> OpenGLWidget::findSeriesAtPosition(float canvas_x, float canvas_y) const {
    // Use CorePlotting SceneHitTester for comprehensive hit testing
    // This queries both spatial index (for discrete elements) and layout (for series regions)

    // Compute layout if dirty (const_cast needed for lazy evaluation pattern)
    // Note: This is safe because computeAndApplyLayout only modifies cache state
    if (_cache_state.layout_response_dirty) {
        const_cast<OpenGLWidget *>(this)->computeAndApplyLayout();
    }

    if (_cache_state.layout_response.layouts.empty()) {
        return std::nullopt;
    }

    // Convert canvas Y to world Y coordinate
    // In OpenGL, Y is inverted: top of window is -1, bottom is +1 in our view
    float const ndc_y = -1.0f + 2.0f * (static_cast<float>(height()) - canvas_y) / static_cast<float>(height());

    // Apply vertical pan offset to get the actual Y position in the coordinate system
    float const world_y = ndc_y - _state->viewState().vertical_pan_offset;
    float const world_x = canvasXToTime(canvas_x);

    // First try full hit test if we have a cached scene with spatial index
    // This can identify specific discrete elements (events, points)
    if (_cache_state.scene.spatial_index) {
        CorePlotting::HitTestResult result = _hit_tester.hitTest(
                world_x, world_y, _cache_state.scene, _cache_state.layout_response);

        if (result.hasHit()) {
            std::string series_type;
            std::string series_key = result.series_key;

            // For discrete hits, look up series key from batch maps if needed
            if (series_key.empty() && result.hit_type == CorePlotting::HitType::DigitalEvent) {
                // Try to find series key from glyph batch maps
                // The QuadTree stores EntityId but not series key, so region query is fallback
                auto region_result = _hit_tester.querySeriesRegion(world_x, world_y, _cache_state.layout_response);
                if (region_result.hasHit()) {
                    series_key = region_result.series_key;
                }
            }

            // Determine series type using data store
            auto series_type_enum = _data_store->findSeriesTypeByKey(series_key);
            switch (series_type_enum) {
                case DataViewer::SeriesType::Analog:
                    series_type = "Analog";
                    break;
                case DataViewer::SeriesType::DigitalEvent:
                    series_type = "Event";
                    break;
                case DataViewer::SeriesType::DigitalInterval:
                    series_type = "Interval";
                    break;
                default:
                    series_type = "Unknown";
                    break;
            }

            return std::make_pair(series_type, series_key);
        }
    }

    // Fall back to series region query (always works, uses layout)
    CorePlotting::HitTestResult result = _hit_tester.querySeriesRegion(
            world_x, world_y, _cache_state.layout_response);

    if (result.hasHit()) {
        // Determine series type using data store
        std::string series_type;
        auto series_type_enum = _data_store->findSeriesTypeByKey(result.series_key);
        switch (series_type_enum) {
            case DataViewer::SeriesType::Analog:
                series_type = "Analog";
                break;
            case DataViewer::SeriesType::DigitalEvent:
                series_type = "Event";
                break;
            case DataViewer::SeriesType::DigitalInterval:
                series_type = "Interval";
                break;
            default:
                series_type = "Unknown";
                break;
        }

        return std::make_pair(series_type, result.series_key);
    }

    return std::nullopt;
}

///////////////////////////////////////////////////////////////////////////////
// New SceneRenderer-based rendering methods
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::renderWithSceneRenderer() {
    if (!_scene_renderer || !_master_time_frame) {
        return;
    }

    // Compute layout if dirty - this updates _cache_state.layout_response
    // and all entry.layout_transform fields in TimeSeriesDataStore
    computeAndApplyLayout();

    auto const & view_state = _state->viewState();
    auto const start_time = TimeFrameIndex(view_state.time_start);
    auto const end_time = TimeFrameIndex(view_state.time_end);

    // Build shared View and Projection matrices
    CorePlotting::ViewProjectionParams view_params;
    view_params.vertical_pan_offset = view_state.vertical_pan_offset;

    // Shared projection matrix (time range to NDC)
    glm::mat4 projection = CorePlotting::getAnalogProjectionMatrix(start_time, end_time, view_state.y_min, view_state.y_max);

    // Shared view matrix (global pan)
    glm::mat4 view = CorePlotting::getAnalogViewMatrix(view_params);

    // Create SceneBuilder and set bounds for spatial indexing
    // Bounds are in world coordinates: X = [start_time, end_time], Y = [y_min, y_max]
    BoundingBox scene_bounds(
            static_cast<float>(start_time.getValue()),// min_x
            view_state.y_min,                        // min_y
            static_cast<float>(end_time.getValue()),  // max_x
            view_state.y_max                         // max_y
    );

    CorePlotting::SceneBuilder builder;
    builder.setBounds(scene_bounds);
    builder.setMatrices(view, projection);
    builder.setSelectedEntities(_selection_manager->selectedEntities());

    // Add all series batches to the scene builder
    addAnalogBatchesToBuilder(builder);
    addEventBatchesToBuilder(builder);
    addIntervalBatchesToBuilder(builder);

    // Build the scene (this also builds spatial index for discrete elements)
    _cache_state.scene = builder.build();
    _cache_state.scene_dirty = false;

    // Store batch key maps for hit testing
    _cache_state.rectangle_batch_key_map = builder.getRectangleBatchKeyMap();
    _cache_state.glyph_batch_key_map = builder.getGlyphBatchKeyMap();

    // Upload scene to renderer and render
    _scene_renderer->uploadScene(_cache_state.scene);
    _scene_renderer->render(view, projection);
}

void OpenGLWidget::addAnalogBatchesToBuilder(CorePlotting::SceneBuilder & builder) {
    if (!_master_time_frame) {
        return;
    }

    auto const & view_state = _state->viewState();
    auto const start_time = TimeFrameIndex(view_state.time_start);
    auto const end_time = TimeFrameIndex(view_state.time_end);

    // Layout has already been computed by computeAndApplyLayout() in renderWithSceneRenderer()
    // Each series' layout is available in _cached_layout_response

    // Access series through data store
    for (auto const & [key, analog_data]: _data_store->analogSeries()) {
        auto const & series = analog_data.series;
        
        // Get display options from state (serializable)
        auto const * opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (!opts || !opts->get_is_visible()) continue;

        // Look up layout from cached response
        auto const * series_layout = _cache_state.layout_response.findLayout(key);
        if (!series_layout) {
            // Fallback to entry's layout_transform if not found (shouldn't happen)
            CorePlotting::SeriesLayout fallback_layout{
                    key,
                    analog_data.layout_transform,
                    0};
            series_layout = &fallback_layout;
        }

        // Compose the full Y transform using data cache from entry and user settings from state
        CorePlotting::LayoutTransform y_transform = DataViewer::composeAnalogYTransform(
                *series_layout,
                analog_data.data_cache.cached_mean,
                analog_data.data_cache.cached_std_dev,
                analog_data.data_cache.intrinsic_scale,
                opts->user_scale_factor,
                opts->y_offset,
                view_state.global_zoom,
                view_state.global_vertical_scale);

        // Create model matrix directly from the composed transform
        glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

        // Parse color from state options
        int r, g, b;
        hexToRGB(opts->hex_color(), r, g, b);

        // Build batch parameters from state options
        DataViewerHelpers::AnalogBatchParams batch_params;
        batch_params.start_time = start_time;
        batch_params.end_time = end_time;
        batch_params.gap_threshold = opts->gap_threshold;
        batch_params.detect_gaps = (opts->gap_handling == AnalogGapHandlingMode::DetectGaps);
        batch_params.color = glm::vec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                opts->get_alpha());
        batch_params.thickness = static_cast<float>(opts->get_line_thickness());

        // Choose render mode based on gap handling setting from state
        if (opts->gap_handling == AnalogGapHandlingMode::ShowMarkers) {
            batch_params.render_mode = DataViewerHelpers::AnalogRenderMode::Markers;
        } else {
            batch_params.render_mode = DataViewerHelpers::AnalogRenderMode::Line;
        }

        // Build and add batch to builder using simplified API (no intermediate param structs)
        if (batch_params.render_mode == DataViewerHelpers::AnalogRenderMode::Markers) {
            auto batch = DataViewerHelpers::buildAnalogSeriesMarkerBatchSimplified(
                    *series, _master_time_frame, batch_params, model_matrix);
            if (!batch.positions.empty()) {
                builder.addGlyphBatch(std::move(batch));
            }
        } else {
            // Use cached batch builder for efficient scrolling
            // The vertex_cache is mutable, allowing updates even with const iteration
            auto batch = DataViewerHelpers::buildAnalogSeriesBatchCached(
                    *series, _master_time_frame, batch_params, model_matrix,
                    analog_data.vertex_cache);
            if (!batch.vertices.empty()) {
                builder.addPolyLineBatch(std::move(batch));
            }
        }
    }
}

void OpenGLWidget::addEventBatchesToBuilder(CorePlotting::SceneBuilder & builder) {
    if (!_master_time_frame) {
        return;
    }

    auto const & view_state = _state->viewState();
    auto const start_time = TimeFrameIndex(view_state.time_start);
    auto const end_time = TimeFrameIndex(view_state.time_end);

    // Layout has already been computed by computeAndApplyLayout() in renderWithSceneRenderer()
    // Each series' layout is available in _cache_state.layout_response

    // Access series through data store
    for (auto const & [key, event_data]: _data_store->eventSeries()) {
        auto const & series = event_data.series;
        
        // Get display options from state (serializable)
        auto const * opts = _state->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (!opts || !opts->get_is_visible()) continue;

        // Determine plotting mode from state options
        bool const is_stacked = (opts->plotting_mode == EventPlottingModeData::Stacked);

        // Compose the Y transform based on plotting mode
        CorePlotting::LayoutTransform y_transform;
        if (!is_stacked) {
            // Full canvas mode - events extend full viewport height
            y_transform = DataViewer::composeEventFullCanvasYTransform(
                    view_state.y_min, view_state.y_max, opts->margin_factor);
        } else {
            // Stacked mode - look up layout from cached response
            auto const * series_layout = _cache_state.layout_response.findLayout(key);
            if (series_layout) {
                y_transform = DataViewer::composeEventYTransform(
                        *series_layout, opts->margin_factor, view_state.global_vertical_scale);
            } else {
                // Fallback if layout not found
                CorePlotting::SeriesLayout fallback{
                        key,
                        event_data.layout_transform,
                        0};
                y_transform = DataViewer::composeEventYTransform(
                        fallback, opts->margin_factor, view_state.global_vertical_scale);
            }
        }

        // Create model matrix from composed transform
        glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

        // Parse color from state options
        int r, g, b;
        hexToRGB(opts->hex_color(), r, g, b);

        // Build batch parameters from state options
        DataViewerHelpers::EventBatchParams batch_params;
        batch_params.start_time = start_time;
        batch_params.end_time = end_time;
        batch_params.color = glm::vec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                opts->get_alpha());
        batch_params.glyph_size = static_cast<float>(opts->get_line_thickness());
        batch_params.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Tick;

        // Build and add batch using simplified API
        auto batch = DataViewerHelpers::buildEventSeriesBatchSimplified(
                *series, _master_time_frame, batch_params, model_matrix);

        if (!batch.positions.empty()) {
            builder.addGlyphBatch(std::move(batch));
        }
    }
}

void OpenGLWidget::addIntervalBatchesToBuilder(CorePlotting::SceneBuilder & builder) {
    if (!_master_time_frame) {
        return;
    }

    auto const & view_state = _state->viewState();
    auto const start_time = TimeFrameIndex(view_state.time_start);
    auto const end_time = TimeFrameIndex(view_state.time_end);

    // Layout has already been computed by computeAndApplyLayout() in renderWithSceneRenderer()
    // Each series' layout is available in _cache_state.layout_response

    // Access series through data store
    for (auto const & [key, interval_data]: _data_store->intervalSeries()) {
        auto const & series = interval_data.series;

        // Get display options from state
        auto const * opts = _state->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
        if (!opts) continue;

        if (!opts->get_is_visible()) continue;

        // Look up layout from cached response
        auto const * series_layout = _cache_state.layout_response.findLayout(key);
        CorePlotting::LayoutTransform y_transform;
        if (series_layout) {
            y_transform = DataViewer::composeIntervalYTransform(
                    *series_layout, opts->margin_factor,
                    view_state.global_zoom, view_state.global_vertical_scale);
        } else {
            // Fallback if layout not found - use entry's layout_transform
            CorePlotting::SeriesLayout fallback{
                    key,
                    interval_data.layout_transform,
                    0};
            y_transform = DataViewer::composeIntervalYTransform(
                    fallback, opts->margin_factor,
                    view_state.global_zoom, view_state.global_vertical_scale);
        }

        // Create model matrix from composed transform
        glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

        // Parse color
        int r, g, b;
        hexToRGB(opts->hex_color(), r, g, b);

        // Build batch parameters
        DataViewerHelpers::IntervalBatchParams batch_params;
        batch_params.start_time = start_time;
        batch_params.end_time = end_time;
        batch_params.color = glm::vec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                opts->get_alpha());

        // Build and add batch using simplified API
        auto batch = DataViewerHelpers::buildIntervalSeriesBatchSimplified(
                *series, _master_time_frame, batch_params, model_matrix);

        if (!batch.bounds.empty()) {
            builder.addRectangleBatch(std::move(batch));
        }
    }
}

// =============================================================================
// Layout System with LayoutEngine)
// =============================================================================

CorePlotting::LayoutRequest OpenGLWidget::buildLayoutRequest() const {
    auto const & view_state = _state->viewState();
    CorePlotting::LayoutRequest request;
    request.viewport_y_min = view_state.y_min;
    request.viewport_y_max = view_state.y_max;

    // Collect visible analog series keys and order by spike sorter config
    // Access series through data store, visibility through state
    std::vector<std::string> visible_analog_keys;
    for (auto const & [key, data]: _data_store->analogSeries()) {
        auto const * opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            visible_analog_keys.push_back(key);
        }
    }

    // Apply spike sorter ordering if any configs exist
    if (!_spike_sorter_configs.empty()) {
        visible_analog_keys = orderKeysBySpikeSorterConfig(visible_analog_keys, _spike_sorter_configs);
    }

    // Add analog series in order
    for (auto const & key: visible_analog_keys) {
        request.series.emplace_back(key, CorePlotting::SeriesType::Analog, true);
    }

    // Add digital event series (stacked events after analog series, full-canvas events as non-stackable)
    for (auto const & [key, data]: _data_store->eventSeries()) {
        auto const * opts = _state->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (!opts || !opts->get_is_visible()) continue;

        bool is_stacked = (opts->plotting_mode == EventPlottingModeData::Stacked);
        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, is_stacked);
    }

    // Add digital interval series (always full-canvas, non-stackable)
    for (auto const & [key, data]: _data_store->intervalSeries()) {
        auto const * opts = _state->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
        if (!opts || !opts->get_is_visible()) continue;

        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalInterval, false);
    }

    return request;
}

void OpenGLWidget::computeAndApplyLayout() {
    if (!_cache_state.layout_response_dirty) {
        return;
    }

    // Build layout request from current series state
    CorePlotting::LayoutRequest request = buildLayoutRequest();

    // Compute layout using LayoutEngine
    _cache_state.layout_response = _layout_engine.compute(request);

    // Apply computed layout to display options via data store
    _data_store->applyLayoutResponse(_cache_state.layout_response);

    // Note: _cache_state.rectangle_batch_key_map is now populated by SceneBuilder in renderWithSceneRenderer()
    // This ensures the batch key map stays synchronized with the actual rendered batches

    _cache_state.layout_response_dirty = false;
}

void OpenGLWidget::loadSpikeSorterConfiguration(std::string const & group_name,
                                                std::vector<ChannelPosition> const & positions) {
    _spike_sorter_configs[group_name] = positions;
    _cache_state.layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::clearSpikeSorterConfiguration(std::string const & group_name) {
    _spike_sorter_configs.erase(group_name);
    _cache_state.layout_response_dirty = true;
    updateCanvas(_time);
}