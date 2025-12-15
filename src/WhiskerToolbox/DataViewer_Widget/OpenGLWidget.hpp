#ifndef OPENGLWIDGET_HPP
#define OPENGLWIDGET_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataViewer/XAxis.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"
#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
//#include <QOpenGLFunctions_4_1_Core>
#include <QEvent>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QTimer>
#include <QToolTip>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
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
class QMouseEvent;
struct LayoutCalculator;

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

    [[nodiscard]] bool isDraggingInterval() const { return _is_dragging_interval; }

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

    void setXLimit(int xmax) {
        _xAxis.setMax(xmax);
    };

    // Accessors for SVG export
    [[nodiscard]] XAxis const & getXAxis() const { return _xAxis; }
    [[nodiscard]] float getYMin() const { return _yMin; }
    [[nodiscard]] float getYMax() const { return _yMax; }
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
     * @param master_time_frame Shared pointer to the master time frame
     */
    void setMasterTimeFrame(std::shared_ptr<TimeFrame> master_time_frame) {
        _master_time_frame = master_time_frame;
    }

    /**
     * @brief Set the PlottingManager reference for coordinate allocation
     * 
     * @param plotting_manager Pointer to PlottingManager managed by DataViewer_Widget
     */
    void setPlottingManager(LayoutCalculator * plotting_manager) {
        _plotting_manager = plotting_manager;
    }

    void changeRangeWidth(int64_t range_delta) {
        int64_t const center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;
        int64_t const current_range = _xAxis.getEnd() - _xAxis.getStart();
        int64_t const new_range = current_range + range_delta;// Add delta to current range
        _xAxis.setCenterAndZoom(center, new_range);
        updateCanvas(_time);
    }

    int64_t setRangeWidth(int64_t range_width) {
        int64_t const center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;
        int64_t const actual_range = _xAxis.setCenterAndZoomWithFeedback(center, range_width);
        updateCanvas(_time);
        return actual_range;// Return the actual range width achieved
    }

    void setGlobalScale(float scale) {

        std::cout << "Global zoom set to " << scale << std::endl;
        _global_zoom = scale;
        updateCanvas(_time);
    }

    [[nodiscard]] std::pair<int, int> getCanvasSize() const {
        return std::make_pair(width(), height());
    }

    /**
     * @brief Enable or disable the new SceneRenderer-based rendering.
     * 
     * When enabled, series are converted to CorePlotting RenderableBatch
     * objects and rendered using the PlottingOpenGL renderers.
     * When disabled, the legacy inline vertex generation is used.
     * 
     * @param enabled True to use new renderer system
     */
    void setUseSceneRenderer(bool enabled) {
        _use_scene_renderer = enabled;
        updateCanvas(_time);
    }

    [[nodiscard]] bool isUsingSceneRenderer() const { return _use_scene_renderer; }

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
    //void generateRandomValues(int count);
    //void generateAndAddFakeData(int count);
    //void adjustFakeData();
    void drawDigitalEventSeries();
    void drawDigitalIntervalSeries();
    void drawAnalogSeries();
    void drawAxis();
    void drawGridLines();
    void drawDashedLine(LineParameters const & params);
    void drawDraggedInterval();
    void drawNewIntervalBeingCreated();
    void _addSeries(std::string const & key);
    void _removeSeries(std::string const & key);
    void _updateYViewBoundaries();

    // New SceneRenderer-based rendering methods
    /**
     * @brief Render all series using the PlottingOpenGL SceneRenderer.
     * 
     * Converts series data to RenderableBatch objects and uses the new
     * renderer system. Called when _use_scene_renderer is true.
     */
    void renderWithSceneRenderer();

    /**
     * @brief Build and upload batches for all visible analog series.
     */
    void uploadAnalogBatches();

    /**
     * @brief Build and upload batches for all visible digital event series.
     */
    void uploadEventBatches();

    /**
     * @brief Build and upload batches for all visible digital interval series.
     */
    void uploadIntervalBatches();

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

    // Gap detection helper methods for analog series
    // These work with TimeValueRangeView which provides the necessary data regardless of underlying storage
    void _drawAnalogSeriesWithGapDetection(std::shared_ptr<TimeFrame> const & time_frame,
                                           AnalogTimeSeries::TimeValueRangeView analog_range,
                                           float gap_threshold);

    void _drawAnalogSeriesAsMarkers(std::shared_ptr<TimeFrame> const & time_frame,
                                    AnalogTimeSeries::TimeValueRangeView analog_range);

    std::unordered_map<std::string, AnalogSeriesData> _analog_series;
    std::unordered_map<std::string, DigitalEventSeriesData> _digital_event_series;
    std::unordered_map<std::string, DigitalIntervalSeriesData> _digital_interval_series;
    std::unordered_map<std::string, int> _series_y_position;

    XAxis _xAxis;
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

    float _global_zoom{1.0f};
    float _verticalPanOffset{0.0f};
    QPoint _lastMousePos{};
    bool _isPanning{false};
    float _yMin{-1.0f};
    float _yMax{1.0f};
    float _ySpacing{0.1f};

    std::string m_background_color{"#000000"};// black
    std::string m_axis_color{"#FFFFFF"};      // white (for dark theme)

    std::vector<GLfloat> m_vertices;// for testing

    PlotTheme _plot_theme{PlotTheme::Dark};

    // Grid line settings
    bool _grid_lines_enabled{false};// Default to disabled
    int _grid_spacing{100};         // Default spacing of 100 time units

    // Interval selection tracking
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> _selected_intervals;

    // Interval dragging state
    bool _is_dragging_interval{false};
    std::string _dragging_series_key;
    bool _dragging_left_edge{false};// true for left edge, false for right edge
    int64_t _original_start_time{0};
    int64_t _original_end_time{0};
    int64_t _dragged_start_time{0};
    int64_t _dragged_end_time{0};
    QPoint _drag_start_pos;

    // Master time frame for X-axis coordinate system
    std::shared_ptr<TimeFrame> _master_time_frame;

    // PlottingManager for coordinate allocation
    LayoutCalculator * _plotting_manager{nullptr};

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

    // Flag to enable/disable using the new renderer system
    // Set to false by default for backwards compatibility during migration
    bool _use_scene_renderer{false};
};

namespace TimeSeriesDefaultValues {

std::vector<std::string> const DEFAULT_COLORS = {
        "#ff0000",// Red
        "#008000",// Green
        "#0000ff",// Blue
        "#ff00ff",// Magenta
        "#ffff00",// Yellow
        "#00ffff",// Cyan
        "#ffa500",// Orange
        "#800080" // Purple
};

// Get color from index, returns random color if index exceeds DEFAULT_COLORS size
std::string getColorForIndex(size_t index);
}// namespace TimeSeriesDefaultValues

#endif//OPENGLWIDGET_HPP
