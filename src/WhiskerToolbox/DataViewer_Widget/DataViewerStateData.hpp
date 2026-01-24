#ifndef DATAVIEWER_STATE_DATA_HPP
#define DATAVIEWER_STATE_DATA_HPP

/**
 * @file DataViewerStateData.hpp
 * @brief Comprehensive serializable state data structure for DataViewer_Widget
 * 
 * This file defines the full state structure that DataViewerState serializes to JSON
 * using reflect-cpp. It captures all persistent state needed for workspace save/restore:
 * 
 * - Display options for all series types (analog, digital events, digital intervals)
 * - View state (time window, Y bounds, zoom, pan)
 * - Theme and grid settings
 * - UI preferences
 * - Active interaction mode
 * 
 * ## Design Principles
 * 
 * 1. **Separation of concerns** - Only user-configurable options are stored here.
 *    Computed state (layout transforms, data caches) stays in TimeSeriesDataStore.
 * 2. **Nested objects for clarity** - Top-level structure uses nested objects for
 *    clear JSON organization
 * 3. **Native enum serialization** - Enums serialize as strings automatically
 * 4. **No Qt types** - All Qt types converted to std equivalents (QString → std::string)
 * 5. **Transient state excluded** - Hover positions, drag state, preview flags NOT included
 * 
 * ## Example JSON Output
 * 
 * ```json
 * {
 *   "instance_id": "abc123",
 *   "display_name": "Data Viewer",
 *   "view": {
 *     "time_start": 0,
 *     "time_end": 10000,
 *     "y_min": -1.0,
 *     "y_max": 1.0,
 *     "global_zoom": 1.5
 *   },
 *   "theme": {
 *     "theme": "Dark",
 *     "background_color": "#000000"
 *   },
 *   "analog_options": {
 *     "channel_1": { "hex_color": "#0000ff", "is_visible": true }
 *   }
 * }
 * ```
 * 
 * @see DataViewerState for the Qt wrapper class
 * @see TimeSeriesDataStore for runtime data storage
 */

#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "CorePlotting/DataTypes/SeriesStyle.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <cstdint>
#include <map>
#include <string>

// ==================== Per-Series Display Options ====================

/**
 * @brief Gap handling mode for analog series
 * 
 * Serializes as "AlwaysConnect", "DetectGaps", or "ShowMarkers" automatically.
 */
enum class AnalogGapHandlingMode {
    AlwaysConnect,  ///< Always connect points (default)
    DetectGaps,     ///< Break lines when gaps exceed threshold
    ShowMarkers     ///< Show individual markers instead of lines
};

/**
 * @brief Serializable display options for analog time series
 * 
 * Contains only user-configurable options. Layout transforms and data caches
 * are computed at runtime and stored in TimeSeriesDataStore.
 */
struct AnalogSeriesOptionsData {
    rfl::Flatten<CorePlotting::SeriesStyle> style;  ///< Visual style (flattened in JSON)
    
    // Analog-specific user settings
    float user_scale_factor = 1.0f;   ///< User-controlled amplitude scaling
    float y_offset = 0.0f;            ///< User-controlled vertical offset
    
    // Gap handling
    AnalogGapHandlingMode gap_handling = AnalogGapHandlingMode::AlwaysConnect;
    bool enable_gap_detection = false;  ///< Enable automatic gap detection
    float gap_threshold = 5.0f;         ///< Threshold for gap detection (in samples)
    
    // Convenience accessors for style fields
    [[nodiscard]] std::string const & hex_color() const { return style.get().hex_color; }
    [[nodiscard]] float get_alpha() const { return style.get().alpha; }
    [[nodiscard]] int get_line_thickness() const { return style.get().line_thickness; }
    [[nodiscard]] bool get_is_visible() const { return style.get().is_visible; }
    
    std::string & hex_color() { return style.get().hex_color; }
    float & alpha() { return style.get().alpha; }
    int & line_thickness() { return style.get().line_thickness; }
    bool & is_visible() { return style.get().is_visible; }
};

/**
 * @brief Plotting mode for digital event series
 * 
 * Serializes as "FullCanvas" or "Stacked" automatically.
 */
enum class EventPlottingModeData {
    FullCanvas,  ///< Events extend full canvas height
    Stacked      ///< Events allocated portion of canvas
};

/**
 * @brief Serializable display options for digital event series
 */
struct DigitalEventSeriesOptionsData {
    rfl::Flatten<CorePlotting::SeriesStyle> style;  ///< Visual style (flattened in JSON)
    
    // Event-specific settings
    EventPlottingModeData plotting_mode = EventPlottingModeData::FullCanvas;
    float vertical_spacing = 0.1f;    ///< Vertical spacing for stacked mode
    float event_height = 0.05f;       ///< Height of individual events
    float margin_factor = 0.95f;      ///< Margin factor (0.95 = 95% of allocated space)
    
    // Convenience accessors for style fields
    [[nodiscard]] std::string const & hex_color() const { return style.get().hex_color; }
    [[nodiscard]] float get_alpha() const { return style.get().alpha; }
    [[nodiscard]] int get_line_thickness() const { return style.get().line_thickness; }
    [[nodiscard]] bool get_is_visible() const { return style.get().is_visible; }
    
    std::string & hex_color() { return style.get().hex_color; }
    float & alpha() { return style.get().alpha; }
    int & line_thickness() { return style.get().line_thickness; }
    bool & is_visible() { return style.get().is_visible; }
};

/**
 * @brief Serializable display options for digital interval series
 */
struct DigitalIntervalSeriesOptionsData {
    rfl::Flatten<CorePlotting::SeriesStyle> style;  ///< Visual style (flattened in JSON)
    
    // Interval-specific settings
    bool extend_full_canvas = true;   ///< Whether intervals extend full canvas
    float margin_factor = 0.95f;      ///< Margin factor
    float interval_height = 1.0f;     ///< Height of interval (1.0 = full)
    
    // Convenience accessors for style fields
    [[nodiscard]] std::string const & hex_color() const { return style.get().hex_color; }
    [[nodiscard]] float get_alpha() const { return style.get().alpha; }
    [[nodiscard]] int get_line_thickness() const { return style.get().line_thickness; }
    [[nodiscard]] bool get_is_visible() const { return style.get().is_visible; }
    
    std::string & hex_color() { return style.get().hex_color; }
    float & alpha() { return style.get().alpha; }
    int & line_thickness() { return style.get().line_thickness; }
    bool & is_visible() { return style.get().is_visible; }
};

// ==================== Theme State ====================

/**
 * @brief Visual theme for the DataViewer
 * 
 * Serializes as "Dark" or "Light" automatically.
 */
enum class DataViewerTheme {
    Dark,   ///< Dark background, light text/axes
    Light   ///< Light background, dark text/axes
};

/**
 * @brief Theme and color settings
 */
struct DataViewerThemeState {
    DataViewerTheme theme = DataViewerTheme::Dark;
    std::string background_color = "#000000";  ///< Background color in hex
    std::string axis_color = "#FFFFFF";        ///< Axis/text color in hex
};

// ==================== Grid State ====================

/**
 * @brief Grid overlay configuration
 */
struct DataViewerGridState {
    bool enabled = false;   ///< Whether grid lines are visible
    int spacing = 100;      ///< Grid spacing in time units
};

// ==================== UI Preferences ====================

/**
 * @brief Zoom scaling mode
 * 
 * Serializes as "Fixed" or "Adaptive" automatically.
 */
enum class DataViewerZoomScalingMode {
    Fixed,     ///< Fixed zoom factor
    Adaptive   ///< Zoom factor scales with current zoom level
};

/**
 * @brief UI layout and behavior preferences
 */
struct DataViewerUIPreferences {
    DataViewerZoomScalingMode zoom_scaling_mode = DataViewerZoomScalingMode::Adaptive;
    bool properties_panel_collapsed = false;
    // Note: splitter sizes not serialized (Qt-specific, layout-dependent)
};

// ==================== Interaction State ====================

/**
 * @brief Interaction mode for the DataViewer
 * 
 * Serializes as "Normal", "CreateInterval", "ModifyInterval", or "CreateLine".
 */
enum class DataViewerInteractionMode {
    Normal,          ///< Default: pan, select, hover tooltips
    CreateInterval,  ///< Click-drag to create a new interval
    ModifyInterval,  ///< Edge dragging to modify existing interval
    CreateLine       ///< Click-drag to draw a selection line
};

/**
 * @brief Current interaction state
 */
struct DataViewerInteractionState {
    DataViewerInteractionMode mode = DataViewerInteractionMode::Normal;
};

// ==================== Main State Structure ====================

/**
 * @brief Complete serializable state for DataViewer_Widget
 * 
 * This struct contains all persistent state that should be saved/restored
 * when serializing a workspace. Transient state (hover positions, active
 * drag operations, vertex caches, etc.) is intentionally excluded.
 * 
 * ## State Categories
 * 
 * | Category | Serialized | Examples |
 * |----------|------------|----------|
 * | Series Options | ✅ Yes | Colors, alpha, visibility, user scale |
 * | View State | ✅ Yes | Time window, Y bounds, zoom |
 * | Theme/Grid | ✅ Yes | Dark/light theme, grid spacing |
 * | UI Preferences | ✅ Yes | Panel state, zoom mode |
 * | Interaction | ✅ Yes | Current tool mode |
 * | Transient State | ❌ No | Vertex caches, layout transforms |
 * 
 * ## Usage
 * 
 * ```cpp
 * DataViewerStateData data;
 * data.view.time_start = 0;
 * data.view.time_end = 10000;
 * data.theme.theme = DataViewerTheme::Dark;
 * 
 * // Add analog series options
 * AnalogSeriesOptionsData opts;
 * opts.hex_color() = "#ff0000";
 * opts.user_scale_factor = 2.0f;
 * data.analog_options["channel_1"] = opts;
 * 
 * // Serialize to JSON
 * auto json = rfl::json::write(data);
 * 
 * // Deserialize from JSON
 * auto result = rfl::json::read<DataViewerStateData>(json);
 * ```
 */
struct DataViewerStateData {
    // === Identity ===
    std::string instance_id;                         ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Viewer";        ///< User-visible name for this widget
    
    // === View State ===
    CorePlotting::TimeSeriesViewState view;          ///< Time window, Y bounds, zoom
    
    // === Theme and Grid ===
    DataViewerThemeState theme;                      ///< Visual theme settings
    DataViewerGridState grid;                        ///< Grid overlay settings
    
    // === UI Preferences ===
    DataViewerUIPreferences ui;                      ///< UI layout preferences
    
    // === Interaction State ===
    DataViewerInteractionState interaction;          ///< Current interaction mode
    
    // === Per-Series Display Options ===
    // Each key is a data key (e.g., "channel_1"), value is the display options.
    // The 'is_visible' field in each options struct indicates if that series is displayed.
    std::map<std::string, AnalogSeriesOptionsData> analog_options;
    std::map<std::string, DigitalEventSeriesOptionsData> event_options;
    std::map<std::string, DigitalIntervalSeriesOptionsData> interval_options;
};

#endif // DATAVIEWER_STATE_DATA_HPP
