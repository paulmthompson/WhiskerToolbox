#ifndef OPENGLWIDGET_HPP
#define OPENGLWIDGET_HPP

/**
 * @file OpenGLWidget.hpp
 * @brief OpenGL-based time-series data visualization widget
 * 
 * @section Architecture
 * 
 * This widget uses a layered architecture separating data, layout, and rendering:
 * 
 * **Layer 1: Data Storage**
 * - Series data stored in typed maps (_analog_series, _digital_event_series, _digital_interval_series)
 * - Each series has associated DisplayOptions controlling appearance and layout
 * 
 * **Layer 2: View State (CorePlotting::TimeSeriesViewState)**
 * - X-axis: TimeRange with bounds-aware scrolling/zooming
 * - Y-axis: y_min/y_max viewport bounds + vertical_pan_offset
 * - Global scaling: global_zoom and global_vertical_scale applied to all series
 * 
 * **Layer 3: Layout (CorePlotting::LayoutEngine)**
 * - Computes vertical positioning for all series via StackedLayoutStrategy
 * - Produces LayoutResponse used for both rendering and hit testing
 * - Replaces the legacy LayoutCalculator with pure CorePlotting API
 * - Supports spike sorter configuration via series ordering in LayoutRequest
 * 
 * **Layer 4: Rendering (PlottingOpenGL::SceneRenderer)**
 * - Converts series data + layout to RenderableBatch objects
 * - Handles polylines (analog), glyphs (events), rectangles (intervals)
 * - Model/View/Projection matrix computation via CorePlotting functions
 * 
 * **Layer 5: Interaction (CorePlotting::IntervalDragController, SceneHitTester)**
 * - Interval edge dragging with constraint enforcement
 * - Series region queries for tooltips and selection
 * - Coordinate transforms (screen ↔ world ↔ time)
 * 
 * @section Phase49Migration Phase 4.9 Migration Notes
 * 
 * The layout system was unified to use CorePlotting::LayoutEngine exclusively.
 * The legacy LayoutCalculator is no longer used. Key changes:
 * 
 * - _layout_engine replaces _plotting_manager for all layout computation
 * - buildLayoutRequest() creates LayoutRequest from current series state
 * - computeAndApplyLayout() computes layout and updates display options
 * - _cached_layout_response is computed directly by LayoutEngine, no manual rebuild
 * - Spike sorter configuration is applied via series ordering in LayoutRequest
 * 
 * @see CorePlotting/DESIGN.md for full architecture details
 * @see CorePlotting/ROADMAP.md for migration history
 */

#include "CorePlotting/CoordinateTransform/TimeRange.hpp" // TimeSeriesViewState
#include "CorePlotting/Interaction/IntervalDragController.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/StackedLayoutStrategy.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"
#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"
#include "SpikeSorterConfigLoader.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


class AnalogTimeSeries;
struct NewAnalogTimeSeriesDisplayOptions;
class DigitalEventSeries;
struct NewDigitalEventSeriesDisplayOptions;
class DigitalIntervalSeries;
struct NewDigitalIntervalSeriesDisplayOptions;
class TimeFrame;
class QEvent;
class QMouseEvent;
class QTimer;

namespace CorePlotting {
class SceneBuilder;
}

struct AnalogSeriesData {
    std::shared_ptr<AnalogTimeSeries> series;
    std::unique_ptr<NewAnalogTimeSeriesDisplayOptions> display_options;
};

struct DigitalEventSeriesData {
    std::shared_ptr<DigitalEventSeries> series;
    std::unique_ptr<NewDigitalEventSeriesDisplayOptions> display_options;
};

struct DigitalIntervalSeriesData {
    std::shared_ptr<DigitalIntervalSeries> series;
    std::unique_ptr<NewDigitalIntervalSeriesDisplayOptions> display_options;
};

struct LineParameters {
    float xStart = 0.0f;
    float xEnd = 0.0f;
    float yStart = 0.0f;
    float yEnd = 0.0f;
    float dashLength = 5.0f;
    float gapLength = 5.0f;
};

enum class PlotTheme {
    Dark,// Black background, white axes (default)
    Light// White background, dark axes
};

//class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core {
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit OpenGLWidget(QWidget * parent = nullptr);

    ~OpenGLWidget() override;

    void addAnalogTimeSeries(
            std::string const & key,
            std::shared_ptr<AnalogTimeSeries> series,
            std::string const & color = "");
    void removeAnalogTimeSeries(std::string const & key);
    void addDigitalEventSeries(
            std::string const & key,
            std::shared_ptr<DigitalEventSeries> series,
            std::string const & color = "");
    void removeDigitalEventSeries(std::string const & key);
    void addDigitalIntervalSeries(
            std::string const & key,
            std::shared_ptr<DigitalIntervalSeries> series,
            std::string const & color = "");
    void removeDigitalIntervalSeries(std::string const & key);
    void clearSeries();
    void setBackgroundColor(std::string const & hexColor);

    void setPlotTheme(PlotTheme theme);
    [[nodiscard]] PlotTheme getPlotTheme() const { return _plot_theme; }

    // Grid line controls
    void setGridLinesEnabled(bool enabled) {
        _grid_lines_enabled = enabled;
        updateCanvas(_time);
    }
    [[nodiscard]] bool getGridLinesEnabled() const { return _grid_lines_enabled; }
    void setGridSpacing(int spacing) {
        _grid_spacing = spacing;
        updateCanvas(_time);
    }
    [[nodiscard]] int getGridSpacing() const { return _grid_spacing; }

    // Vertical spacing controls for analog series
    void setVerticalSpacing(float spacing) {
        _ySpacing = spacing;
        updateCanvas(_time);
    }
    [[nodiscard]] float getVerticalSpacing() const { return _ySpacing; }

    // Interval selection and manipulation controls

    /**
     * @brief Set the highlighted interval for a digital interval series
     * 
     * @param series_key The key identifying the digital interval series
     * @param start_time Start time of the interval in master time frame coordinates
     * @param end_time End time of the interval in master time frame coordinates
     */
    void setSelectedInterval(std::string const & series_key, int64_t start_time, int64_t end_time);

    /**
     * @brief Clear the highlighted interval for a digital interval series
     * 
     * @param series_key The key identifying the digital interval series
     */
    void clearSelectedInterval(std::string const & series_key);

    /**
     * @brief Get the currently highlighted interval for a digital interval series
     * 
     * @param series_key The key identifying the digital interval series
     * @return Optional pair containing start and end times in master time frame coordinates,
     *         or nullopt if no interval is selected
     */
    std::optional<std::pair<int64_t, int64_t>> getSelectedInterval(std::string const & series_key) const;

    /**
     * @brief Find an interval at the specified time coordinate
     * 
     * Searches for digital intervals that contain the given time coordinate.
     * Automatically handles time frame conversion when the series uses a different
     * time frame from the master time frame.
     * 
     * @param series_key The key identifying the digital interval series
     * @param time_coord Time coordinate in master time frame coordinates
     * @return Optional pair containing start and end times in master time frame coordinates,
     *         or nullopt if no interval is found at the specified time
     * 
     * @note Time frame conversion is performed internally:
     *       - Input time_coord is in master time frame coordinates
     *       - Conversion to series time frame is done for data queries
     *       - Returned interval bounds are converted back to master time frame coordinates
     */
    std::optional<std::pair<int64_t, int64_t>> findIntervalAtTime(std::string const & series_key, float time_coord) const;

    // Interval edge dragging controls

    /**
     * @brief Find interval edges near the specified canvas position
     * 
     * @param canvas_x Canvas X coordinate in pixels
     * @param canvas_y Canvas Y coordinate in pixels  
     * @return Optional pair containing series key and edge type (true=left, false=right),
     *         or nullopt if no edge is found near the position
     */
    std::optional<std::pair<std::string, bool>> findIntervalEdgeAtPosition(float canvas_x, float canvas_y) const;

    /**
     * @brief Start dragging an interval edge
     * 
     * Initiates interval edge dragging for the specified series. The dragging system
     * automatically handles time frame conversion when the series uses a different
     * time frame from the master time frame.
     * 
     * @param series_key The key identifying the digital interval series
     * @param is_left_edge True for left edge, false for right edge
     * @param start_pos Initial mouse position when drag started
     * 
     * @note Time frame handling:
     *       - Mouse coordinates are converted from master time frame to series time frame
     *       - Collision detection is performed in the series' native time frame
     *       - Display coordinates remain in master time frame for consistent rendering
     */
    void startIntervalDrag(std::string const & series_key, bool is_left_edge, QPoint const & start_pos);

    /**
     * @brief Update the dragged interval position
     * 
     * Updates the position of the interval being dragged based on current mouse position.
     * Handles collision detection and constraint enforcement in the series' native time frame.
     * 
     * @param current_pos Current mouse position
     * 
     * @note Error handling:
     *       - If time frame conversion fails, the drag operation is aborted
     *       - If series data becomes invalid, the drag operation is cancelled
     *       - Invalid interval bounds are rejected and the drag state is preserved
     */
    void updateIntervalDrag(QPoint const & current_pos);

    /**
     * @brief Complete the interval dragging operation
     * 
     * Finalizes the interval drag by updating the actual data in the digital interval series.
     * All coordinate conversions between master time frame and series time frame are
     * handled automatically.
     * 
     * @note Error handling:
     *       - If coordinate conversion fails, the operation is aborted
     *       - If data update fails, the original interval is preserved
     *       - The drag state is always cleared regardless of success/failure
     * 
     * @note Time frame conversion:
     *       - Master time frame coordinates are converted to series time frame indices
     *       - Data operations are performed in the series' native time frame
     *       - Selection tracking remains in master time frame coordinates
     */
    void finishIntervalDrag();

    /**
     * @brief Cancel the current interval drag operation
     * 
     * Cancels any ongoing interval drag operation and returns to normal state.
     * This resets all drag-related state without applying any changes.
     */
    void cancelIntervalDrag();

    [[nodiscard]] bool isDraggingInterval() const { return _interval_drag_controller.isActive(); }

    // New interval creation controls

    /**
     * @brief Start creating a new interval by double-clicking and dragging
     * 
     * Initiates the creation of a new interval starting at the specified position.
     * The user can then drag left or right to define the interval bounds.
     * 
     * @param series_key The key identifying the digital interval series
     * @param start_pos Initial mouse position where double-click occurred
     */
    void startNewIntervalCreation(std::string const & series_key, QPoint const & start_pos);

    /**
     * @brief Update the new interval being created
     * 
     * Updates the bounds of the interval being created based on current mouse position.
     * Visual feedback is provided similar to interval edge dragging.
     * 
     * @param current_pos Current mouse position
     */
    void updateNewIntervalCreation(QPoint const & current_pos);

    /**
     * @brief Finish creating the new interval
     * 
     * Completes the new interval creation process by adding the interval to the series.
     * The interval start and end times are automatically ordered correctly.
     */
    void finishNewIntervalCreation();

    /**
     * @brief Cancel the new interval creation process
     * 
     * Cancels the current new interval creation and returns to normal state.
     */
    void cancelNewIntervalCreation();

    [[nodiscard]] bool isCreatingNewInterval() const { return _is_creating_new_interval; }

    /**
     * @brief Set the maximum X-axis bound (typically from TimeFrame count)
     * @param xmax Maximum value for time range (typically total frame count - 1)
     */
    void setXLimit(int xmax);


    // Accessors for SVG export and external queries
    /**
     * @brief Get the current view state (time window and Y-axis state)
     * @return Reference to CorePlotting::TimeSeriesViewState with current state
     */
    [[nodiscard]] CorePlotting::TimeSeriesViewState const & getViewState() const { return _view_state; }


    [[nodiscard]] float getYMin() const { return _view_state.y_min; }
    [[nodiscard]] float getYMax() const { return _view_state.y_max; }
    [[nodiscard]] std::string const & getBackgroundColor() const { return m_background_color; }
    [[nodiscard]] std::shared_ptr<TimeFrame> getMasterTimeFrame() const { return _master_time_frame; }
    [[nodiscard]] auto const & getAnalogSeriesMap() const { return _analog_series; }
    [[nodiscard]] auto const & getDigitalEventSeriesMap() const { return _digital_event_series; }
    [[nodiscard]] auto const & getDigitalIntervalSeriesMap() const { return _digital_interval_series; }

    /**
     * @brief Set the master time frame used for X-axis coordinates
     * 
     * The master time frame defines the coordinate system for the X-axis display.
     * Individual data series may have different time frames that need to be
     * converted to/from the master time frame for proper synchronization.
     * 
     * Also initializes the TimeRange bounds from the TimeFrame extent.
     * 
     * @param master_time_frame Shared pointer to the master time frame
     */
    void setMasterTimeFrame(std::shared_ptr<TimeFrame> master_time_frame);

    /**
     * @brief Load spike sorter configuration for a group of analog series
     * 
     * Applies custom ordering to analog series within a group based on
     * physical electrode positions from spike sorting software.
     * Series are ordered by Y position (ascending) for vertical stacking.
     * 
     * @param group_name Group identifier (series keys should be "groupname_N")
     * @param positions Vector of channel positions for ordering
     */
    void loadSpikeSorterConfiguration(std::string const & group_name,
                                      std::vector<ChannelPosition> const & positions);

    /**
     * @brief Clear spike sorter configuration for a group
     * @param group_name Group identifier to clear
     */
    void clearSpikeSorterConfiguration(std::string const & group_name);

    /**
     * @brief Change the visible range width by a delta amount
     * @param range_delta Amount to add to current range (negative = zoom in)
     */
    void changeRangeWidth(int64_t range_delta);

    /**
     * @brief Set the visible range width to a specific value
     * @param range_width Desired range width
     * @return Actual range width achieved (may differ due to bounds clamping)
     */
    int64_t setRangeWidth(int64_t range_width);

    void setGlobalScale(float scale) {

        std::cout << "Global zoom set to " << scale << std::endl;
        _view_state.global_zoom = scale;
        updateCanvas(_time);
    }

    /**
     * @brief Set global vertical scale factor
     * @param scale Vertical scale factor (1.0 = normal)
     */
    void setGlobalVerticalScale(float scale) {
        _view_state.global_vertical_scale = scale;
        updateCanvas(_time);
    }

    /**
     * @brief Get current global zoom factor
     * @return Global zoom value
     */
    [[nodiscard]] float getGlobalZoom() const { return _view_state.global_zoom; }

    /**
     * @brief Get current global vertical scale
     * @return Global vertical scale value
     */
    [[nodiscard]] float getGlobalVerticalScale() const { return _view_state.global_vertical_scale; }

    /**
     * @brief Get current vertical pan offset
     * @return Vertical pan offset in NDC units
     */
    [[nodiscard]] float getVerticalPanOffset() const { return _view_state.vertical_pan_offset; }

    [[nodiscard]] std::pair<int, int> getCanvasSize() const {
        return std::make_pair(width(), height());
    }

    // Coordinate conversion methods
    [[nodiscard]] float canvasXToTime(float canvas_x) const;
    [[nodiscard]] float canvasYToAnalogValue(float canvas_y, std::string const & series_key) const;

    // Display options getters (similar to Media_Window pattern)
    [[nodiscard]] std::optional<NewAnalogTimeSeriesDisplayOptions *> getAnalogConfig(std::string const & key) const {
        auto it = _analog_series.find(key);
        if (it != _analog_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<NewDigitalEventSeriesDisplayOptions *> getDigitalEventConfig(std::string const & key) const {
        auto it = _digital_event_series.find(key);
        if (it != _digital_event_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<NewDigitalIntervalSeriesDisplayOptions *> getDigitalIntervalConfig(std::string const & key) const {
        auto it = _digital_interval_series.find(key);
        if (it != _digital_interval_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

public slots:
    void updateCanvas() { updateCanvas(_time); }
    void updateCanvas(int time);

signals:
    void mouseHover(float time_coordinate, float canvas_y, QString const & series_info);
    void mouseClick(float time_coordinate, float canvas_y, QString const & series_info);

protected:
    void initializeGL() override;
    void paintGL() override;

    void resizeGL(int w, int h) override;

    void cleanup();

    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void leaveEvent(QEvent * event) override;

private:
    void setupVertexAttribs();
    void drawAxis();
    void drawGridLines();
    void drawDashedLine(LineParameters const & params);
    void drawDraggedInterval();
    void drawNewIntervalBeingCreated();

    // New SceneRenderer-based rendering methods
    /**
     * @brief Render all series using the PlottingOpenGL SceneRenderer.
     * 
     * Uses SceneBuilder to construct a RenderableScene and uploads it
     * to SceneRenderer for rendering. The batch key maps from SceneBuilder
     * are preserved for hit testing.
     */
    void renderWithSceneRenderer();

    /**
     * @brief Add batches for all visible analog series to the scene builder.
     * @param builder The SceneBuilder to add batches to
     */
    void addAnalogBatchesToBuilder(CorePlotting::SceneBuilder & builder);

    /**
     * @brief Add batches for all visible digital event series to the scene builder.
     * @param builder The SceneBuilder to add batches to
     */
    void addEventBatchesToBuilder(CorePlotting::SceneBuilder & builder);

    /**
     * @brief Add batches for all visible digital interval series to the scene builder.
     * @param builder The SceneBuilder to add batches to
     */
    void addIntervalBatchesToBuilder(CorePlotting::SceneBuilder & builder);

    // Tooltip helper methods
    /**
     * @brief Find the series under the mouse cursor
     * 
     * Checks analog series and digital event series (in stacked mode) to determine
     * which series is under the given canvas coordinates.
     * 
     * @param canvas_x X coordinate in canvas pixels
     * @param canvas_y Y coordinate in canvas pixels
     * @return Optional pair containing series type ("analog" or "event") and series key,
     *         or nullopt if no series is under the cursor
     */
    std::optional<std::pair<std::string, std::string>> findSeriesAtPosition(float canvas_x, float canvas_y) const;

    /**
     * @brief Show tooltip with series information after hover delay
     */
    void showSeriesInfoTooltip(QPoint const & pos);

    /**
     * @brief Start the tooltip timer on mouse hover
     */
    void startTooltipTimer(QPoint const & pos);

    /**
     * @brief Cancel the tooltip timer
     */
    void cancelTooltipTimer();

    std::unordered_map<std::string, AnalogSeriesData> _analog_series;
    std::unordered_map<std::string, DigitalEventSeriesData> _digital_event_series;
    std::unordered_map<std::string, DigitalIntervalSeriesData> _digital_interval_series;

    // X-axis state using CorePlotting TimeSeriesViewState (Phase 4.6 migration)
    // Replaces legacy XAxis class with bounds-aware TimeRange + unified ViewState
    CorePlotting::TimeSeriesViewState _view_state;
    int _time{0};

    QOpenGLShaderProgram * m_program{nullptr};
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;
    QMatrix4x4 m_proj; // Initialized as identity
    QMatrix4x4 m_view; // Initialized as identity
    QMatrix4x4 m_model;// Initialized as identity
    int m_projMatrixLoc{};
    int m_viewMatrixLoc{};
    int m_modelMatrixLoc{};
    int m_colorLoc{};
    int m_alphaLoc{};

    QOpenGLShaderProgram * m_dashedProgram{nullptr};
    int m_dashedProjMatrixLoc{};
    int m_dashedViewMatrixLoc{};
    int m_dashedModelMatrixLoc{};
    int m_dashedResolutionLoc{};
    int m_dashedDashSizeLoc{};
    int m_dashedGapSizeLoc{};

    QPoint _lastMousePos{};
    bool _isPanning{false};
    float _ySpacing{0.1f};///< Vertical spacing factor for series

    std::string m_background_color{"#000000"};// black
    std::string m_axis_color{"#FFFFFF"};      // white (for dark theme)

    PlotTheme _plot_theme{PlotTheme::Dark};

    // Grid line settings
    bool _grid_lines_enabled{false};// Default to disabled
    int _grid_spacing{100};         // Default spacing of 100 time units

    // Interval selection tracking
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> _selected_intervals;

    // Interval dragging state (uses CorePlotting::IntervalDragController)
    CorePlotting::IntervalDragController _interval_drag_controller;

    // Master time frame for X-axis coordinate system
    std::shared_ptr<TimeFrame> _master_time_frame;

    // Layout engine for coordinate allocation (Phase 4.9 migration)
    // Uses StackedLayoutStrategy for DataViewer-style vertical stacking
    CorePlotting::LayoutEngine _layout_engine{
            std::make_unique<CorePlotting::StackedLayoutStrategy>()};

    // Spike sorter configuration for custom series ordering
    // Maps group_name -> vector of channel positions
    SpikeSorterConfigMap _spike_sorter_configs;

    // New interval creation state
    bool _is_creating_new_interval{false};
    std::string _new_interval_series_key;
    int64_t _new_interval_start_time{0};
    int64_t _new_interval_end_time{0};
    int64_t _new_interval_click_time{0};// Time coordinate where double-click occurred
    QPoint _new_interval_click_pos;

    // ShaderManager integration
    ShaderSourceType m_shaderSourceType = ShaderSourceType::Resource;
    void setShaderSourceType(ShaderSourceType type) { m_shaderSourceType = type; }

    // GL lifecycle guards
    bool _gl_initialized{false};
    QMetaObject::Connection _ctxAboutToBeDestroyedConn;

    // Tooltip state
    QTimer * _tooltip_timer{nullptr};
    QPoint _tooltip_hover_pos;
    static constexpr int TOOLTIP_DELAY_MS = 1000;///< Delay before showing tooltip (1 second)

    // PlottingOpenGL Renderers
    // These use the new CorePlotting RenderableBatch approach for rendering.
    // SceneRenderer coordinates all batch renderers (polylines, glyphs, rectangles).
    std::unique_ptr<PlottingOpenGL::SceneRenderer> _scene_renderer;

    // CorePlotting hit testing infrastructure (Phase 4.11 - Complete SceneHitTester Integration)
    // The hit tester provides unified hit testing via SceneHitTester
    CorePlotting::SceneHitTester _hit_tester;

    // Cached scene for hit testing - stores the last rendered scene for spatial queries
    // This is populated in renderWithSceneRenderer() and used by findIntervalEdgeAtPosition()
    CorePlotting::RenderableScene _cached_scene;
    bool _scene_dirty{true};///< True when scene needs to be rebuilt before hit testing

    // Cached layout response - computed by LayoutEngine when dirty
    // Used for both rendering (updating display options) and hit testing
    CorePlotting::LayoutResponse _cached_layout_response;
    bool _layout_response_dirty{true};

    // Mapping from rectangle batch index to series key (for interval hit testing)
    std::map<size_t, std::string> _rectangle_batch_key_map;

    // Mapping from glyph batch index to series key (for event hit testing)
    std::map<size_t, std::string> _glyph_batch_key_map;

    /**
     * @brief Build a LayoutRequest from current series state
     * 
     * Creates a LayoutRequest with all visible series, applying spike sorter
     * configuration for custom ordering when present.
     * 
     * @return LayoutRequest ready for LayoutEngine::compute()
     */
    [[nodiscard]] CorePlotting::LayoutRequest buildLayoutRequest() const;

    /**
     * @brief Compute layout and apply results to display options
     * 
     * Recomputes _cached_layout_response using LayoutEngine and updates
     * all series display_options->layout fields with computed positions.
     * Called automatically when _layout_response_dirty is true.
     */
    void computeAndApplyLayout();

};

/**
 * @brief Default values and utilities for time series display configuration
 */
namespace TimeSeriesDefaultValues {

inline constexpr std::array<char const *, 8> DEFAULT_COLORS = {
        "#0000ff",// Blue
        "#ff0000",// Red
        "#00ff00",// Green
        "#ff00ff",// Magenta
        "#ffff00",// Yellow
        "#00ffff",// Cyan
        "#ffa500",// Orange
        "#800080" // Purple
};

/**
 * @brief Get color from index, returns hash-based color if index exceeds DEFAULT_COLORS size
 * @param index Index of the color to retrieve
 * @return Hex color string
 */
inline std::string getColorForIndex(size_t index) {
    if (index < DEFAULT_COLORS.size()) {
        return DEFAULT_COLORS[index];
    }
    // Generate a pseudo-random color based on index
    unsigned int const hash = static_cast<unsigned int>(index) * 2654435761u;
    int const r = static_cast<int>((hash >> 16) & 0xFF);
    int const g = static_cast<int>((hash >> 8) & 0xFF);
    int const b = static_cast<int>(hash & 0xFF);

    char hex_buffer[8];
    std::snprintf(hex_buffer, sizeof(hex_buffer), "#%02x%02x%02x", r, g, b);
    return std::string(hex_buffer);
}

}// namespace TimeSeriesDefaultValues

#endif//OPENGLWIDGET_HPP
