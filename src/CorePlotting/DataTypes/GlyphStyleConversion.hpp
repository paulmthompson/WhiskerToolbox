#ifndef COREPLOTTING_DATATYPES_GLYPHSTYLECONVERSION_HPP
#define COREPLOTTING_DATATYPES_GLYPHSTYLECONVERSION_HPP

/**
 * @file GlyphStyleConversion.hpp
 * @brief Conversion utilities between serializable GlyphStyleData and rendering GlyphStyle
 *
 * Provides free functions to convert the user-facing, serializable GlyphStyleData
 * to the rendering-level GlyphStyle consumed by SceneBuilder::addGlyphs().
 *
 * This header deliberately includes both GlyphStyleData.hpp (serializable)
 * and SceneBuilder.hpp (rendering) so widget code only needs one include
 * for the full pipeline.
 */

#include "GlyphStyleData.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <charconv>
#include <string_view>

namespace CorePlotting {

// Forward declare the rendering style (defined in SceneBuilder.hpp)
struct GlyphStyle;

/**
 * @brief Convert a user-facing GlyphType to the rendering-level GlyphType
 *
 * Types not yet supported by the renderer (Diamond, Triangle) fall back
 * to Circle so rendering never fails.
 *
 * @param type User-facing glyph type
 * @return Rendering-level glyph type
 */
[[nodiscard]] inline RenderableGlyphBatch::GlyphType toRenderableGlyphType(GlyphType type) {
    switch (type) {
        case GlyphType::Circle:   return RenderableGlyphBatch::GlyphType::Circle;
        case GlyphType::Square:   return RenderableGlyphBatch::GlyphType::Square;
        case GlyphType::Tick:     return RenderableGlyphBatch::GlyphType::Tick;
        case GlyphType::Cross:    return RenderableGlyphBatch::GlyphType::Cross;
        case GlyphType::Diamond:  return RenderableGlyphBatch::GlyphType::Circle;  // fallback
        case GlyphType::Triangle: return RenderableGlyphBatch::GlyphType::Circle;  // fallback
    }
    return RenderableGlyphBatch::GlyphType::Circle;
}

/**
 * @brief Parse a hex color string to RGBA glm::vec4
 *
 * Supports "#RRGGBB" format. Returns white on parse failure.
 *
 * @param hex Hex color string (e.g., "#ff0000")
 * @param alpha Alpha override [0.0, 1.0]
 * @return RGBA color as glm::vec4
 */
[[nodiscard]] inline glm::vec4 hexColorToVec4(std::string_view hex, float alpha = 1.0f) {
    if (hex.size() < 7 || hex[0] != '#') {
        return glm::vec4{1.0f, 1.0f, 1.0f, alpha};
    }

    auto parse_byte = [](std::string_view sv) -> int {
        int value = 0;
        std::from_chars(sv.data(), sv.data() + sv.size(), value, 16);
        return value;
    };

    float const r = static_cast<float>(parse_byte(hex.substr(1, 2))) / 255.0f;
    float const g = static_cast<float>(parse_byte(hex.substr(3, 2))) / 255.0f;
    float const b = static_cast<float>(parse_byte(hex.substr(5, 2))) / 255.0f;

    return glm::vec4{r, g, b, std::clamp(alpha, 0.0f, 1.0f)};
}

}// namespace CorePlotting

#endif// COREPLOTTING_DATATYPES_GLYPHSTYLECONVERSION_HPP
