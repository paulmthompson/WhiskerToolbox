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
 * **Layer 5: Interaction (RectangleInteractionController, SceneHitTester)**
 * - Unified glyph interaction via IGlyphInteractionController
 * - Interval creation and edge dragging via RectangleInteractionController
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
#include "CorePlotting/Interaction/IGlyphInteractionController.hpp"
#include "CorePlotting/Interaction/DataCoordinates.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/StackedLayoutStrategy.hpp"
#include "DataViewerCoordinates.hpp"
#include "DataViewerInputHandler.hpp"
#include "DataViewerInteractionManager.hpp"
#include "DataViewerSelectionManager.hpp"
#include "DataViewerTooltipController.hpp"
#include "PlottingOpenGL/Renderers/AxisRenderer.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"
#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"
#include "SpikeSorterConfigLoader.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeSeriesDataStore.hpp"

#include <QMatrix4x4>
#include <QOpenGLFunctions>
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
class QEvent;
class QMouseEvent;
class QTimer;

namespace CorePlotting {
class SceneBuilder;
}

enum class PlotTheme {
    Dark,// Black background, white axes (default)
    Light// White background, dark axes
};

/**
 * @brief Theme-related state for the OpenGLWidget
 * 
 * Groups visual theme settings including background and axis colors.
 */
struct ThemeState {
    PlotTheme theme{PlotTheme::Dark};
    std::string background_color{"#000000"}; // black for dark theme
    std::string axis_color{"#FFFFFF"};       // white for dark theme
};

/**
 * @brief Grid overlay settings
 * 
 * Groups grid line configuration for time-axis aligned vertical lines.
 */
struct GridState {
    bool enabled{false};   // Default to disabled
    int spacing{100};      // Default spacing of 100 time units
};

/**
 * @brief Cached rendering and hit-testing state
 * 
 * Groups scene caching for efficient rendering and spatial queries.
 */
struct SceneCacheState {
    CorePlotting::RenderableScene scene;
    CorePlotting::LayoutResponse layout_response;
    std::map<size_t, std::string> rectangle_batch_key_map; // interval series keys
    std::map<size_t, std::string> glyph_batch_key_map;     // event series keys
    bool scene_dirty{true};           // True when scene needs rebuild
    bool layout_response_dirty{true}; // True when layout needs recompute
};

/**
 * @brief OpenGL resource state
 * 
 * Groups OpenGL lifecycle resources including matrices.
 */
struct OpenGLResourceState {
    QMatrix4x4 proj;  // Projection matrix (identity default)
    QMatrix4x4 view;  // View matrix (identity default)
    bool initialized{false};
    QMetaObject::Connection ctx_about_to_be_destroyed_conn;
    ShaderSourceType shader_source_type{ShaderSourceType::Resource};
};

// Use InteractionMode from DataViewer namespace
using DataViewer::InteractionMode;

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
    [[nodiscard]] PlotTheme getPlotTheme() const { return _theme_state.theme; }

    // Grid line controls
    void setGridLinesEnabled(bool enabled) {
        _grid_state.enabled = enabled;
        updateCanvas(_time);
    }
    [[nodiscard]] bool getGridLinesEnabled() const { return _grid_state.enabled; }
    void setGridSpacing(int spacing) {
        _grid_state.spacing = spacing;
        updateCanvas(_time);
    }
    [[nodiscard]] int getGridSpacing() const { return _grid_state.spacing; }

    // ========================================================================
    // EntityId-based Selection API (preferred for new code)
    // ========================================================================

    /**
     * @brief Select an entity by its EntityId
     * 
     * Adds the entity to the selection set. Use with Ctrl+click for multi-select.
     * 
     * @param id EntityId to select
     */
    void selectEntity(EntityId id);

    /**
     * @brief Deselect an entity by its EntityId
     * @param id EntityId to deselect
     */
    void deselectEntity(EntityId id);

    /**
     * @brief Toggle selection state of an entity
     * @param id EntityId to toggle
     */
    void toggleEntitySelection(EntityId id);

    /**
     * @brief Clear all selected entities
     */
    void clearEntitySelection();

    /**
     * @brief Check if an entity is selected
     * @param id EntityId to check
     * @return true if the entity is in the selection set
     */
    [[nodiscard]] bool isEntitySelected(EntityId id) const;

    /**
     * @brief Get all selected entities
     * @return Const reference to the selection set
     */
    [[nodiscard]] std::unordered_set<EntityId> const & getSelectedEntities() const;

    // ========================================================================
    // Unified Interaction Mode API
    // ========================================================================

    /**
     * @brief Set the current interaction mode
     * 
     * Changes how mouse events are interpreted. When switching modes,
     * any active interaction is cancelled.
     * 
     * @param mode The new interaction mode
     */
    void setInteractionMode(InteractionMode mode);

    /**
     * @brief Get the current interaction mode
     */
    [[nodiscard]] InteractionMode interactionMode() const;

    /**
     * @brief Check if any interaction is currently active
     * 
     * Returns true if the widget is in the middle of creating or modifying
     * a glyph (interval, line, etc.).
     */
    [[nodiscard]] bool isInteractionActive() const;

    /**
     * @brief Cancel any active interaction
     * 
     * Resets to Normal mode without committing any changes.
     */
    void cancelActiveInteraction();

    // Accessors for SVG export and external queries
    /**
     * @brief Get the current view state (time window and Y-axis state)
     * @return Reference to CorePlotting::TimeSeriesViewState with current state
     */
    [[nodiscard]] CorePlotting::TimeSeriesViewState const & getViewState() const { return _view_state; }

    [[nodiscard]] std::string const & getBackgroundColor() const { return _theme_state.background_color; }
    [[nodiscard]] std::shared_ptr<TimeFrame> getMasterTimeFrame() const { return _master_time_frame; }
    [[nodiscard]] auto const & getAnalogSeriesMap() const { return _data_store->analogSeries(); }
    [[nodiscard]] auto const & getDigitalEventSeriesMap() const { return _data_store->eventSeries(); }
    [[nodiscard]] auto const & getDigitalIntervalSeriesMap() const { return _data_store->intervalSeries(); }

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

    [[nodiscard]] std::pair<int, int> getCanvasSize() const {
        return std::make_pair(width(), height());
    }

    // Coordinate conversion methods
    [[nodiscard]] float canvasXToTime(float canvas_x) const;
    [[nodiscard]] float canvasYToAnalogValue(float canvas_y, std::string const & series_key) const;

    // Display options getters (delegates to TimeSeriesDataStore)
    [[nodiscard]] std::optional<NewAnalogTimeSeriesDisplayOptions *> getAnalogConfig(std::string const & key) {
        return _data_store->getAnalogConfig(key);
    }

    [[nodiscard]] std::optional<NewDigitalEventSeriesDisplayOptions *> getDigitalEventConfig(std::string const & key) {
        return _data_store->getEventConfig(key);
    }

    [[nodiscard]] std::optional<NewDigitalIntervalSeriesDisplayOptions *> getDigitalIntervalConfig(std::string const & key) {
        return _data_store->getIntervalConfig(key);
    }

public slots:
    void updateCanvas() { updateCanvas(_time); }
    void updateCanvas(TimeFrameIndex time);

signals:
    void mouseHover(float time_coordinate, float canvas_y, QString const & series_info);
    void mouseClick(float time_coordinate, float canvas_y, QString const & series_info);
    
    /**
     * @brief Emitted when an entity is selected or deselected
     * @param entity_id The EntityId that was selected/deselected
     * @param is_selected true if selected, false if deselected
     */
    void entitySelectionChanged(EntityId entity_id, bool is_selected);

    /**
     * @brief Emitted when the interaction mode changes
     * @param mode The new interaction mode
     */
    void interactionModeChanged(InteractionMode mode);

    /**
     * @brief Emitted when an interaction completes successfully
     * 
     * The DataCoordinates contain all information needed to update the DataManager.
     * Connect to this signal to handle the result of interactive operations.
     * 
     * @param coords The data coordinates resulting from the interaction
     */
    void interactionCompleted(CorePlotting::Interaction::DataCoordinates const & coords);

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
    void drawAxis();
    void drawGridLines();
    
    /**
     * @brief Draw the preview from the active glyph controller
     * 
     * If _glyph_controller is active, renders its preview using the
     * SceneRenderer's PreviewRenderer. Used for interval creation,
     * line selection, and other interactive glyph operations.
     */
    void drawInteractionPreview();

    /**
     * @brief Handle completed interaction and update DataManager
     * 
     * Handles both interval creation and modification. Converts the
     * DataCoordinates to series-native time frame and updates the data.
     * 
     * @param coords The data coordinates from the completed interaction
     */
    void handleInteractionCompleted(CorePlotting::Interaction::DataCoordinates const & coords);

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

    /**
     * @brief Find the series under the mouse cursor
     * 
     * Checks analog series and digital event series (in stacked mode) to determine
     * which series is under the given canvas coordinates. Used by tooltip controller
     * and input handler for series identification.
     * 
     * @param canvas_x X coordinate in canvas pixels
     * @param canvas_y Y coordinate in canvas pixels
     * @return Optional pair containing series type ("Analog", "Event", "Interval") and series key,
     *         or nullopt if no series is under the cursor
     */
    std::optional<std::pair<std::string, std::string>> findSeriesAtPosition(float canvas_x, float canvas_y) const;
    
    /// Centralized storage for all time series data
    std::unique_ptr<DataViewer::TimeSeriesDataStore> _data_store;

    CorePlotting::TimeSeriesViewState _view_state;
    TimeFrameIndex _time{0};

    
    /// OpenGL resources: shader, buffers, matrices
    OpenGLResourceState _gl_state;

    /// Theme settings: colors and visual style
    ThemeState _theme_state;

    /// Grid overlay configuration
    GridState _grid_state;

    /// Cached scene and layout for rendering/hit testing
    SceneCacheState _cache_state;

    // ========================================================================
    // Input, Interaction, Selection, and Tooltip Handlers
    // (Extracted from widget for cleaner separation of concerns)
    // ========================================================================
    
    /// Handles mouse events and emits semantic signals
    std::unique_ptr<DataViewer::DataViewerInputHandler> _input_handler;
    
    /// Manages interaction state machine for interval creation/modification
    std::unique_ptr<DataViewer::DataViewerInteractionManager> _interaction_manager;

    /// Manages entity selection state (multi-select supported)
    std::unique_ptr<DataViewer::DataViewerSelectionManager> _selection_manager;

    /// Manages tooltip display with hover delay
    std::unique_ptr<DataViewer::DataViewerTooltipController> _tooltip_controller;

    // Master time frame for X-axis coordinate system
    std::shared_ptr<TimeFrame> _master_time_frame;

    // Layout engine for coordinate allocation
    // Uses StackedLayoutStrategy for DataViewer-style vertical stacking
    CorePlotting::LayoutEngine _layout_engine{
            std::make_unique<CorePlotting::StackedLayoutStrategy>()};

    // Spike sorter configuration for custom series ordering
    // Maps group_name -> vector of channel positions
    SpikeSorterConfigMap _spike_sorter_configs;

    // PlottingOpenGL Renderers
    // These use the new CorePlotting RenderableBatch approach for rendering.
    // SceneRenderer coordinates all batch renderers (polylines, glyphs, rectangles).
    std::unique_ptr<PlottingOpenGL::SceneRenderer> _scene_renderer;

    // AxisRenderer for axis lines and grid overlays
    std::unique_ptr<PlottingOpenGL::AxisRenderer> _axis_renderer;

    // The hit tester provides unified hit testing via SceneHitTester
    CorePlotting::SceneHitTester _hit_tester;

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

#endif//OPENGLWIDGET_HPP
