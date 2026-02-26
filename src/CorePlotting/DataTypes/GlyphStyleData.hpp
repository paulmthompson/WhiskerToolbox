#ifndef COREPLOTTING_DATATYPES_GLYPHSTYLEDATA_HPP
#define COREPLOTTING_DATATYPES_GLYPHSTYLEDATA_HPP

/**
 * @file GlyphStyleData.hpp
 * @brief Serializable glyph style configuration for point/marker rendering
 *
 * This header defines the canonical glyph style types used across all plot widgets
 * that render discrete markers (points, events, scatter dots, etc.).
 *
 * **Design Goals**:
 * - Plain struct with no Qt dependencies → includable in state data structs
 * - Directly serializable with reflectcpp (enums as strings, simple fields)
 * - Superset of all per-widget glyph enums (replaces EventGlyphType, etc.)
 * - Convertible to the rendering-level CorePlotting::GlyphStyle for SceneBuilder
 *
 * **Usage Pattern** (mirrors VerticalAxisStateData):
 * Include this struct as a member in your widget's StateData:
 * @code
 *   struct MyWidgetStateData {
 *       CorePlotting::GlyphStyleData glyph_style;           // single global style
 *       std::map<std::string, CorePlotting::GlyphStyleData> per_key_styles; // per-series
 *   };
 * @endcode
 *
 * @see GlyphStyleState for the QObject wrapper with signals
 * @see SeriesStyle for line-oriented visual properties
 * @see RenderableGlyphBatch::GlyphType for the rendering-level enum
 */

#include <string>

namespace CorePlotting {

/**
 * @brief Marker shape for discrete point/event rendering
 *
 * This is the user-facing glyph type that gets serialized to JSON.
 * It is a superset of all widget-specific glyph enums and maps 1:1
 * to RenderableGlyphBatch::GlyphType values (plus additional types
 * that may be added to the renderer in the future).
 *
 * reflectcpp serializes this as a string (e.g., "Circle", "Tick").
 */
enum class GlyphType {
    Circle,   ///< Filled circle marker
    Square,   ///< Filled square marker
    Tick,     ///< Vertical line (for event raster plots)
    Cross,    ///< '+' shaped marker
    Diamond,  ///< Diamond (rotated square) — future renderer support
    Triangle  ///< Triangle marker — future renderer support
};

/**
 * @brief Serializable glyph style configuration
 *
 * Contains all user-configurable visual properties for rendering
 * discrete markers. This struct is designed to be embedded directly
 * in plot state data structs for reflectcpp serialization.
 *
 * All fields have sensible defaults so a default-constructed
 * GlyphStyleData produces visible Circle markers.
 */
struct GlyphStyleData {
    GlyphType glyph_type = GlyphType::Circle; ///< Marker shape
    float size = 5.0f;                         ///< Marker size in pixels
    std::string hex_color = "#007bff";         ///< Color as hex string (e.g., "#007bff")
    float alpha = 1.0f;                        ///< Alpha transparency [0.0, 1.0]
};

}// namespace CorePlotting

#endif// COREPLOTTING_DATATYPES_GLYPHSTYLEDATA_HPP
