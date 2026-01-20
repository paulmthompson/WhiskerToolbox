#ifndef MEDIA_WIDGET_STATE_DATA_HPP
#define MEDIA_WIDGET_STATE_DATA_HPP

/**
 * @file MediaWidgetStateData.hpp
 * @brief Comprehensive serializable state data structure for MediaWidget
 * 
 * This file defines the full state structure that MediaWidgetState serializes to JSON
 * using reflect-cpp. It captures all persistent state needed for workspace save/restore:
 * 
 * - Display options for all data types (lines, masks, points, tensors, intervals, media)
 * - Viewport state (zoom, pan, canvas size)
 * - Interaction preferences (line tools, mask brush, point selection)
 * - Text overlays
 * - Active tool modes
 * 
 * ## Design Principles
 * 
 * 1. **Nested objects for clarity** - Unlike DisplayOptions which use rfl::Flatten,
 *    this top-level structure uses nested objects for clear JSON organization
 * 2. **Native enum serialization** - Enums serialize as strings automatically
 * 3. **No Qt types** - All Qt types converted to std equivalents (QString → std::string)
 * 4. **Transient state excluded** - Hover positions, drag state, preview flags are NOT included
 * 
 * ## Example JSON Output
 * 
 * ```json
 * {
 *   "instance_id": "abc123",
 *   "display_name": "Media Viewer",
 *   "displayed_media_key": "video.mp4",
 *   "viewport": {
 *     "zoom": 2.0,
 *     "pan_x": 100.0,
 *     "pan_y": 50.0
 *   },
 *   "line_options": {
 *     "whisker_1": { "hex_color": "#ff0000", "line_thickness": 3 }
 *   },
 *   "active_line_mode": "Add"
 * }
 * ```
 * 
 * @see MediaWidgetState for the Qt wrapper class
 * @see DisplayOptions.hpp for per-feature display option types
 */

#include "DisplayOptions/DisplayOptions.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <string>
#include <vector>

// ==================== Text Overlay Data ====================

/**
 * @brief Orientation for text overlays
 * 
 * Serializes as "Horizontal" or "Vertical" automatically.
 * Matches the TextOrientation enum in MediaText_Widget.hpp
 */
enum class TextOverlayOrientation {
    Horizontal, ///< Normal horizontal text
    Vertical    ///< Rotated 90 degrees for vertical display
};

/**
 * @brief Serializable text overlay data
 * 
 * This mirrors the TextOverlay struct in MediaText_Widget.hpp but uses
 * std::string instead of QString/QColor for serialization.
 */
struct TextOverlayData {
    int id = -1;                                              ///< Unique identifier (-1 = not assigned)
    std::string text;                                         ///< Text content to display
    TextOverlayOrientation orientation = TextOverlayOrientation::Horizontal;  ///< Text orientation
    float x_position = 0.5f;                                  ///< Relative X position (0.0 - 1.0)
    float y_position = 0.5f;                                  ///< Relative Y position (0.0 - 1.0)
    std::string color = "#ffffff";                            ///< Text color in hex format
    int font_size = 12;                                       ///< Font size in points
    bool enabled = true;                                      ///< Whether overlay is visible
};

// ==================== Interaction Preferences ====================

/**
 * @brief User preferences for line interaction tools
 * 
 * These are tool settings that persist across sessions, not transient
 * state like "currently drawing" or "drag in progress".
 */
struct LineInteractionPrefs {
    std::string smoothing_mode = "SimpleSmooth";  ///< "SimpleSmooth" or "PolynomialFit"
    int polynomial_order = 3;                      ///< Order for polynomial fit (2-10)
    bool edge_snapping_enabled = false;           ///< Whether to snap new points to edges
    int edge_threshold = 100;                      ///< Canny edge detection threshold
    int edge_search_radius = 20;                   ///< Radius in pixels for edge search
    int eraser_radius = 10;                        ///< Radius in pixels for line eraser
    float selection_threshold = 15.0f;            ///< Pixel distance for line selection
};

/**
 * @brief User preferences for mask interaction tools
 */
struct MaskInteractionPrefs {
    int brush_size = 15;              ///< Brush size in pixels
    bool hover_circle_visible = true; ///< Show brush preview circle on hover
    bool allow_empty_mask = false;    ///< Whether to preserve empty masks during brush removal
};

/**
 * @brief User preferences for point interaction
 */
struct PointInteractionPrefs {
    float selection_threshold = 10.0f;  ///< Pixel distance for point selection
};

// ==================== Viewport State ====================

/**
 * @brief Viewport/camera state for the media display
 * 
 * Captures zoom level, pan position, and canvas dimensions for
 * restoring the exact view the user had.
 */
struct ViewportState {
    double zoom = 1.0;        ///< Zoom factor (1.0 = no zoom)
    double pan_x = 0.0;       ///< Horizontal pan offset in pixels
    double pan_y = 0.0;       ///< Vertical pan offset in pixels
    int canvas_width = 640;   ///< Canvas width in pixels
    int canvas_height = 480;  ///< Canvas height in pixels
};

// ==================== Tool Mode Enums ====================

/**
 * @brief Active mode for line tools
 * 
 * Serializes as "None", "Add", "Erase", "Select", or "DrawAllFrames"
 */
enum class LineToolMode {
    None,          ///< No line tool active
    Add,           ///< Adding points to line
    Erase,         ///< Erasing points from line
    Select,        ///< Selecting lines
    DrawAllFrames  ///< Drawing across all frames
};

/**
 * @brief Active mode for mask tools
 * 
 * Serializes as "None" or "Brush"
 */
enum class MaskToolMode {
    None,   ///< No mask tool active
    Brush   ///< Brush painting mode
};

/**
 * @brief Active mode for point tools
 * 
 * Serializes as "None" or "Select"
 */
enum class PointToolMode {
    None,   ///< No point tool active
    Select  ///< Point selection mode
};

// ==================== Main State Structure ====================

/**
 * @brief Complete serializable state for MediaWidget
 * 
 * This struct contains all persistent state that should be saved/restored
 * when serializing a workspace. Transient state (hover positions, active
 * drag operations, preview masks, etc.) is intentionally excluded.
 * 
 * ## State Categories
 * 
 * | Category | Serialized | Examples |
 * |----------|------------|----------|
 * | Display Options | ✅ Yes | Colors, alpha, line thickness |
 * | Viewport | ✅ Yes | Zoom, pan, canvas size |
 * | Tool Preferences | ✅ Yes | Brush size, edge snapping |
 * | Active Tool Mode | ✅ Yes | Current line/mask/point mode |
 * | Text Overlays | ✅ Yes | Labels and annotations |
 * | Transient State | ❌ No | Hover position, drag state |
 * 
 * ## Usage
 * 
 * ```cpp
 * MediaWidgetStateData data;
 * data.displayed_media_key = "video.mp4";
 * data.viewport.zoom = 2.0;
 * 
 * // Add line display options
 * LineDisplayOptions line_opts;
 * line_opts.hex_color() = "#ff0000";
 * line_opts.line_thickness = 3;
 * data.line_options["whisker_1"] = line_opts;
 * 
 * // Serialize to JSON
 * auto json = rfl::json::write(data);
 * 
 * // Deserialize from JSON
 * auto result = rfl::json::read<MediaWidgetStateData>(json);
 * ```
 */
struct MediaWidgetStateData {
    // === Identity ===
    std::string instance_id;                        ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Media Viewer";      ///< User-visible name for this widget
    
    // === Primary Display ===
    std::string displayed_media_key;                ///< Primary media/video data key being displayed
    
    // === Viewport (nested object in JSON) ===
    ViewportState viewport;                         ///< Zoom, pan, and canvas state
    
    // === Per-Feature Display Options ===
    // Each key is a data key (e.g., "whisker_1"), value is the display options.
    // The 'is_visible' field in each options struct indicates if that feature is enabled.
    std::map<std::string, MediaDisplayOptions> media_options;               ///< Media/image display settings
    std::map<std::string, LineDisplayOptions> line_options;                 ///< Line display settings
    std::map<std::string, MaskDisplayOptions> mask_options;                 ///< Mask display settings
    std::map<std::string, PointDisplayOptions> point_options;               ///< Point display settings
    std::map<std::string, DigitalIntervalDisplayOptions> interval_options;  ///< Interval display settings
    std::map<std::string, TensorDisplayOptions> tensor_options;             ///< Tensor display settings
    
    // === Interaction Preferences (nested objects in JSON) ===
    LineInteractionPrefs line_prefs;   ///< Line tool preferences
    MaskInteractionPrefs mask_prefs;   ///< Mask tool preferences
    PointInteractionPrefs point_prefs; ///< Point tool preferences
    
    // === Text Overlays ===
    std::vector<TextOverlayData> text_overlays;  ///< All text overlays
    int next_overlay_id = 0;                     ///< Counter for assigning overlay IDs
    
    // === Active Tool State ===
    LineToolMode active_line_mode = LineToolMode::None;    ///< Current line tool mode
    MaskToolMode active_mask_mode = MaskToolMode::None;    ///< Current mask tool mode
    PointToolMode active_point_mode = PointToolMode::None; ///< Current point tool mode
};

#endif // MEDIA_WIDGET_STATE_DATA_HPP
