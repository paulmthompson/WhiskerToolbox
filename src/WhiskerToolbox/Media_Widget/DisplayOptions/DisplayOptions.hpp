#ifndef DISPLAY_OPTIONS_HPP
#define DISPLAY_OPTIONS_HPP

/**
 * @file DisplayOptions.hpp
 * @brief Display option structs for Media_Widget visualization
 * 
 * This file defines serializable display options for various data types
 * displayed in the Media_Widget. All structs are designed for reflect-cpp
 * serialization using rfl::Flatten for composition instead of inheritance.
 * 
 * ## Design Principles
 * 
 * 1. **No inheritance** - reflect-cpp doesn't handle inherited members well
 * 2. **Composition via rfl::Flatten** - CommonDisplayFields is flattened into each struct
 * 3. **Native enum serialization** - enums serialize as strings automatically
 * 4. **Default constructible** - all members have sensible defaults
 * 
 * ## Usage
 * 
 * ```cpp
 * LineDisplayOptions opts;
 * opts.hex_color = "#ff0000";  // Direct access (recommended)
 * opts.alpha = 0.8f;
 * opts.line_thickness = 3;
 * 
 * // Serialize to JSON
 * auto json = rfl::json::write(opts);
 * 
 * // Deserialize from JSON
 * auto restored = rfl::json::read<LineDisplayOptions>(json);
 * ```
 * 
 * @see MediaWidgetState for state management
 * @see MediaWidgetStateData for full widget state serialization
 */

#include "CorePlotting/Layout/CanvasCoordinateSystem.hpp"
#include "CorePlotting/DataTypes/GlyphStyleData.hpp"
#include "CoreUtilities/color.hpp"
#include "ImageProcessing/ProcessingOptions.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>
#include <vector>

// ==================== Enums ====================

/**
 * @brief Style for displaying digital intervals
 */
enum class IntervalPlottingStyle {
    Box,  ///< Box indicators in corners
    Border///< Border around entire image
};

/**
 * @brief Location for interval box display
 */
enum class IntervalLocation {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

// ==================== Default Values ====================

namespace DefaultDisplayValues {
std::string const COLOR = "#007bff";
float const ALPHA = 1.0f;
bool const VISIBLE = false;
int const POINT_SIZE = 5;
int const LINE_THICKNESS = 2;
int const TENSOR_DISPLAY_CHANNEL = 0;
bool const SHOW_POINTS = false;
CorePlotting::GlyphType const POINT_MARKER_SHAPE = CorePlotting::GlyphType::Circle;

std::vector<std::string> const DEFAULT_COLORS = {
        "#ff0000",// Red
        "#008000",// Green
        "#00ffff",// Cyan
        "#ff00ff",// Magenta
        "#ffff00" // Yellow
};

/**
 * @brief Get color from index, returns random color if index exceeds DEFAULT_COLORS size
 */
inline std::string getColorForIndex(size_t index) {
    if (index < DEFAULT_COLORS.size()) {
        return DEFAULT_COLORS[index];
    } else {
        return generateRandomColor();
    }
}
}// namespace DefaultDisplayValues

// ==================== Common Display Fields ====================

/**
 * @brief Common fields shared by all display option types
 * 
 * This struct is designed to be used with rfl::Flatten to compose
 * into other display option structs. The fields will appear flat
 * in the JSON output (not nested under "common").
 * 
 * Example JSON output when flattened:
 * ```json
 * {
 *   "hex_color": "#ff0000",
 *   "alpha": 0.8,
 *   "is_visible": true,
 *   "line_thickness": 3
 * }
 * ```
 */
struct CommonDisplayFields {
    std::string hex_color{DefaultDisplayValues::COLOR};///< Color in hex format (e.g., "#ff0000")
    float alpha{DefaultDisplayValues::ALPHA};          ///< Alpha/opacity (0.0 - 1.0)
    bool is_visible{DefaultDisplayValues::VISIBLE};    ///< Whether the data is currently visible
};

// ==================== Display Options (Composition via rfl::Flatten) ====================

/**
 * @brief Display options for media (images/video) data
 * 
 * Contains common display fields plus image processing options.
 * Note: MagicEraserOptions is included but contains runtime mask data
 * that won't serialize cleanly (the mask vector).
 */
struct MediaDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;///< Flattened common fields

    // Processing chain active flags (managed at this level, not inside option structs)
    bool contrast_active{false};    ///< Whether contrast filter is active
    bool gamma_active{false};       ///< Whether gamma filter is active
    bool sharpen_active{false};     ///< Whether sharpen filter is active
    bool clahe_active{false};       ///< Whether CLAHE filter is active
    bool bilateral_active{false};   ///< Whether bilateral filter is active
    bool median_active{false};      ///< Whether median filter is active
    bool magic_eraser_active{false};///< Whether magic eraser is active
    bool colormap_active{false};    ///< Whether colormap is active

    ContrastOptions contrast_options;     ///< Contrast/brightness settings
    GammaOptions gamma_options;           ///< Gamma correction settings
    SharpenOptions sharpen_options;       ///< Sharpening settings
    ClaheOptions clahe_options;           ///< CLAHE settings
    BilateralOptions bilateral_options;   ///< Bilateral filter settings
    MedianOptions median_options;         ///< Median filter settings
    MagicEraserParams magic_eraser_params;///< Magic eraser UI-editable parameters
    MagicEraserState magic_eraser_state;  ///< Magic eraser runtime state (not serialized cleanly)
    ColormapOptions colormap_options;     ///< Colormap settings

    // === Convenience accessors for common fields ===
    [[nodiscard]] std::string & hex_color() { return common.get().hex_color; }
    [[nodiscard]] std::string const & hex_color() const { return common.get().hex_color; }
    [[nodiscard]] float & alpha() { return common.get().alpha; }
    [[nodiscard]] float const & alpha() const { return common.get().alpha; }
    [[nodiscard]] bool & is_visible() { return common.get().is_visible; }
    [[nodiscard]] bool const & is_visible() const { return common.get().is_visible; }
};

/**
 * @brief Display options for point data
 */
struct PointDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;///< Flattened common fields

    int point_size{DefaultDisplayValues::POINT_SIZE};                              ///< Size of point markers in pixels
    CorePlotting::GlyphType marker_shape{DefaultDisplayValues::POINT_MARKER_SHAPE};///< Shape of point markers
    CoordinateMappingMode coordinate_mapping{CoordinateMappingMode::ScaleToCanvas};///< How data coordinates map to canvas

    // === Convenience accessors for common fields ===
    [[nodiscard]] std::string & hex_color() { return common.get().hex_color; }
    [[nodiscard]] std::string const & hex_color() const { return common.get().hex_color; }
    [[nodiscard]] float & alpha() { return common.get().alpha; }
    [[nodiscard]] float const & alpha() const { return common.get().alpha; }
    [[nodiscard]] bool & is_visible() { return common.get().is_visible; }
    [[nodiscard]] bool const & is_visible() const { return common.get().is_visible; }
};

/**
 * @brief Display options for line/polyline data
 */
struct LineDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;///< Flattened common fields

    int line_thickness{DefaultDisplayValues::LINE_THICKNESS};///< Line width in pixels
    bool show_points{DefaultDisplayValues::SHOW_POINTS};     ///< Show points as open circles along the line
    bool edge_snapping{false};                               ///< Enable edge snapping for new points
    CoordinateMappingMode coordinate_mapping{CoordinateMappingMode::ScaleToCanvas};///< How data coordinates map to canvas
    bool show_position_marker{false};                        ///< Show position marker at percentage distance
    int position_percentage{20};                             ///< Percentage distance along line (0-100%)
    bool show_segment{false};                                ///< Show only a segment of the line
    int segment_start_percentage{0};                         ///< Start percentage for line segment (0-100%)
    int segment_end_percentage{100};                         ///< End percentage for line segment (0-100%)
    int selected_line_index{-1};                             ///< Index of selected line (-1 = none selected)

    // === Convenience accessors for common fields ===
    [[nodiscard]] std::string & hex_color() { return common.get().hex_color; }
    [[nodiscard]] std::string const & hex_color() const { return common.get().hex_color; }
    [[nodiscard]] float & alpha() { return common.get().alpha; }
    [[nodiscard]] float const & alpha() const { return common.get().alpha; }
    [[nodiscard]] bool & is_visible() { return common.get().is_visible; }
    [[nodiscard]] bool const & is_visible() const { return common.get().is_visible; }
};

/**
 * @brief Display options for mask data
 */
struct MaskDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;///< Flattened common fields

    bool show_bounding_box{false};  ///< Show bounding box around the mask
    bool show_outline{false};       ///< Show outline of the mask as a thick line
    bool use_as_transparency{false};///< Use mask as transparency layer (invert display)
    CoordinateMappingMode coordinate_mapping{CoordinateMappingMode::ScaleToCanvas};///< How data coordinates map to canvas

    // === Convenience accessors for common fields ===
    [[nodiscard]] std::string & hex_color() { return common.get().hex_color; }
    [[nodiscard]] std::string const & hex_color() const { return common.get().hex_color; }
    [[nodiscard]] float & alpha() { return common.get().alpha; }
    [[nodiscard]] float const & alpha() const { return common.get().alpha; }
    [[nodiscard]] bool & is_visible() { return common.get().is_visible; }
    [[nodiscard]] bool const & is_visible() const { return common.get().is_visible; }
};

/**
 * @brief Display options for tensor data
 */
struct TensorDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;///< Flattened common fields

    int display_channel{DefaultDisplayValues::TENSOR_DISPLAY_CHANNEL};///< Channel to display

    // === Convenience accessors for common fields ===
    [[nodiscard]] std::string & hex_color() { return common.get().hex_color; }
    [[nodiscard]] std::string const & hex_color() const { return common.get().hex_color; }
    [[nodiscard]] float & alpha() { return common.get().alpha; }
    [[nodiscard]] float const & alpha() const { return common.get().alpha; }
    [[nodiscard]] bool & is_visible() { return common.get().is_visible; }
    [[nodiscard]] bool const & is_visible() const { return common.get().is_visible; }
};

/**
 * @brief Display options for digital interval data
 */
struct DigitalIntervalDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;///< Flattened common fields

    IntervalPlottingStyle plotting_style{IntervalPlottingStyle::Box};///< Overall plotting style

    // Box style specific options
    int box_size{20};                                     ///< Size of each interval box in pixels
    int frame_range{2};                                   ///< Number of frames before/after current (+/- range)
    IntervalLocation location{IntervalLocation::TopRight};///< Location of interval boxes

    // Border style specific options
    int border_thickness{5};///< Thickness of border in pixels

    // === Convenience accessors for common fields ===
    [[nodiscard]] std::string & hex_color() { return common.get().hex_color; }
    [[nodiscard]] std::string const & hex_color() const { return common.get().hex_color; }
    [[nodiscard]] float & alpha() { return common.get().alpha; }
    [[nodiscard]] float const & alpha() const { return common.get().alpha; }
    [[nodiscard]] bool & is_visible() { return common.get().is_visible; }
    [[nodiscard]] bool const & is_visible() const { return common.get().is_visible; }
};

// ==================== Backwards Compatibility ====================

/**
 * @brief Legacy alias for BaseDisplayOptions
 * @deprecated Use CommonDisplayFields with rfl::Flatten instead
 * 
 * This alias is provided for backwards compatibility during migration.
 * New code should not use inheritance - use composition with rfl::Flatten.
 */
using BaseDisplayOptions = CommonDisplayFields;

#endif// DISPLAY_OPTIONS_HPP
