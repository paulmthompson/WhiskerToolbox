#ifndef OPENGLWIDGET_HPP
#define OPENGLWIDGET_HPP

#include "XAxis.hpp"
#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
//#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;
class QMouseEvent;

struct AnalogSeriesData {
    std::shared_ptr<AnalogTimeSeries> series;
    std::shared_ptr<TimeFrame> time_frame;
    std::unique_ptr<AnalogTimeSeriesDisplayOptions> display_options;
};

struct DigitalEventSeriesData {
    std::shared_ptr<DigitalEventSeries> series;
    std::shared_ptr<TimeFrame> time_frame;
    std::unique_ptr<DigitalEventSeriesDisplayOptions> display_options;
};

struct DigitalIntervalSeriesData {
    std::shared_ptr<DigitalIntervalSeries> series;
    std::shared_ptr<TimeFrame> time_frame;
    std::unique_ptr<DigitalIntervalSeriesDisplayOptions> display_options;
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
    Dark,   // Black background, white axes (default)
    Light   // White background, dark axes
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
            std::shared_ptr<TimeFrame> time_frame,
            std::string const & color = "");
    void removeAnalogTimeSeries(std::string const & key);
    void addDigitalEventSeries(
            std::string const & key,
            std::shared_ptr<DigitalEventSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string const & color = "");
    void removeDigitalEventSeries(std::string const & key);
    void addDigitalIntervalSeries(
            std::string const & key,
            std::shared_ptr<DigitalIntervalSeries> series,
            std::shared_ptr<TimeFrame> time_frame,
            std::string const & color = "");
    void removeDigitalIntervalSeries(std::string const & key);
    void clearSeries();
    void setBackgroundColor(std::string const & hexColor);

    void setPlotTheme(PlotTheme theme);
    [[nodiscard]] PlotTheme getPlotTheme() const { return _plot_theme; }

    // Grid line controls
    void setGridLinesEnabled(bool enabled) { _grid_lines_enabled = enabled; updateCanvas(_time); }
    [[nodiscard]] bool getGridLinesEnabled() const { return _grid_lines_enabled; }
    void setGridSpacing(int spacing) { _grid_spacing = spacing; updateCanvas(_time); }
    [[nodiscard]] int getGridSpacing() const { return _grid_spacing; }

    // Vertical spacing controls for analog series
    void setVerticalSpacing(float spacing) { _ySpacing = spacing; updateCanvas(_time); }
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
     * @brief Cancel the interval dragging operation
     * 
     * Cancels the current drag operation without making any changes to the data.
     * The original interval remains unchanged.
     */
    void cancelIntervalDrag();

    [[nodiscard]] bool isDraggingInterval() const { return _is_dragging_interval; }

    void setXLimit(int xmax) {
        _xAxis.setMax(xmax);
    };

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

    void changeRangeWidth(int64_t range_delta) {
        int64_t const center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;
        int64_t const current_range = _xAxis.getEnd() - _xAxis.getStart();
        int64_t const new_range = current_range + range_delta; // Add delta to current range
        _xAxis.setCenterAndZoom(center, new_range);
        updateCanvas(_time);
    }

    int64_t setRangeWidth(int64_t range_width) {
        int64_t const center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;
        int64_t const actual_range = _xAxis.setCenterAndZoomWithFeedback(center, range_width);
        updateCanvas(_time);
        return actual_range; // Return the actual range width achieved
    }

    [[nodiscard]] XAxis getXAxis() const { return _xAxis; }

    void setGlobalScale(float scale) {
        _global_zoom = scale;
        updateCanvas(_time);
    }

    [[nodiscard]] std::pair<int, int> getCanvasSize() const {
        return std::make_pair(width(), height());
    }

    // Coordinate conversion methods
    [[nodiscard]] float canvasXToTime(float canvas_x) const;
    [[nodiscard]] float canvasYToAnalogValue(float canvas_y, std::string const & series_key) const;

    // Display options getters (similar to Media_Window pattern)
    [[nodiscard]] std::optional<AnalogTimeSeriesDisplayOptions *> getAnalogConfig(std::string const & key) const {
        auto it = _analog_series.find(key);
        if (it != _analog_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<DigitalEventSeriesDisplayOptions *> getDigitalEventConfig(std::string const & key) const {
        auto it = _digital_event_series.find(key);
        if (it != _digital_event_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<DigitalIntervalSeriesDisplayOptions *> getDigitalIntervalConfig(std::string const & key) const {
        auto it = _digital_interval_series.find(key);
        if (it != _digital_interval_series.end()) {
            return it->second.display_options.get();
        }
        return std::nullopt;
    }

public slots:
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
    void _addSeries(std::string const & key);
    void _removeSeries(std::string const & key);
    void _updateYViewBoundaries();
    
    // Gap detection helper methods for analog series
    template<typename Iterator>
    void _drawAnalogSeriesWithGapDetection(Iterator start_it, Iterator end_it,
                                          std::vector<float> const & data,
                                          std::vector<size_t> const & data_time,
                                          std::shared_ptr<TimeFrame> const & time_frame,
                                          float gap_threshold,
                                          float rNorm, float gNorm, float bNorm);
    
    template<typename Iterator>
    void _drawAnalogSeriesAsMarkers(Iterator start_it, Iterator end_it,
                                   std::vector<float> const & data,
                                   std::vector<size_t> const & data_time,
                                   std::shared_ptr<TimeFrame> const & time_frame,
                                   float rNorm, float gNorm, float bNorm);

    // Gap analysis for automatic display mode selection
    struct GapAnalysis {
        bool has_gaps{false};
        int gap_count{0};
        float max_gap_size{0.0f};
        float mean_gap_size{0.0f};
        float recommended_threshold{5.0f};
    };
    
    GapAnalysis _analyzeDataGaps(AnalogTimeSeries const & series);

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
    std::string m_axis_color{"#FFFFFF"};// white (for dark theme)

    std::vector<GLfloat> m_vertices;// for testing

    PlotTheme _plot_theme{PlotTheme::Dark};

    // Grid line settings
    bool _grid_lines_enabled{false}; // Default to disabled
    int _grid_spacing{100}; // Default spacing of 100 time units

    // Interval selection tracking
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> _selected_intervals;

    // Interval dragging state
    bool _is_dragging_interval{false};
    std::string _dragging_series_key;
    bool _dragging_left_edge{false}; // true for left edge, false for right edge
    int64_t _original_start_time{0};
    int64_t _original_end_time{0};
    int64_t _dragged_start_time{0};
    int64_t _dragged_end_time{0};
    QPoint _drag_start_pos;
    
    // Master time frame for X-axis coordinate system
    std::shared_ptr<TimeFrame> _master_time_frame;
};


#endif//OPENGLWIDGET_HPP
