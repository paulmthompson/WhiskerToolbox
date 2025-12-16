#ifndef COREPLOTTING_MAPPERS_MAPPEDELEMENT_HPP
#define COREPLOTTING_MAPPERS_MAPPEDELEMENT_HPP

#include "Entity/EntityTypes.hpp"

#include <glm/glm.hpp>

namespace CorePlotting {

/**
 * @brief Common element type for discrete entities (events, points)
 * 
 * Yielded by mappers when transforming discrete data sources.
 * Contains world-space coordinates and entity identification.
 * 
 * @note X and Y coordinates are in world space after layout transforms
 *       have been applied by the mapper.
 */
struct MappedElement {
    float x;            ///< X position in world space
    float y;            ///< Y position in world space  
    EntityId entity_id; ///< Entity identifier for hit testing

    MappedElement() = default;

    MappedElement(float x_, float y_, EntityId id)
        : x(x_), y(y_), entity_id(id) {}
    
    /**
     * @brief Convert to glm::vec2 for direct use with rendering
     */
    [[nodiscard]] glm::vec2 position() const {
        return {x, y};
    }
};

/**
 * @brief Element type for rectangles (intervals)
 * 
 * Yielded by mappers when transforming interval-based data sources.
 * Contains world-space bounds and entity identification.
 */
struct MappedRectElement {
    float x;            ///< Left edge X position in world space
    float y;            ///< Bottom edge Y position in world space
    float width;        ///< Width in world space
    float height;       ///< Height in world space
    EntityId entity_id; ///< Entity identifier for hit testing

    MappedRectElement() = default;

    MappedRectElement(float x_, float y_, float w, float h, EntityId id)
        : x(x_), y(y_), width(w), height(h), entity_id(id) {}

    /**
     * @brief Get bounds as glm::vec4 (x, y, width, height)
     * 
     * Direct compatibility with RenderableRectangleBatch::bounds format.
     */
    [[nodiscard]] glm::vec4 bounds() const {
        return {x, y, width, height};
    }

    /**
     * @brief Get center position
     */
    [[nodiscard]] glm::vec2 center() const {
        return {x + width / 2.0f, y + height / 2.0f};
    }
};

/**
 * @brief Single vertex in a polyline (analog series, line data)
 * 
 * Yields world-space coordinates for line rendering.
 * Note: EntityId is typically per-line, not per-vertex, so it's tracked
 * at the MappedLineView level rather than per-vertex.
 */
struct MappedVertex {
    float x;  ///< X position in world space
    float y;  ///< Y position in world space

    MappedVertex() = default;

    MappedVertex(float x_, float y_)
        : x(x_), y(y_) {}

    /**
     * @brief Convert to glm::vec2 for direct use with rendering
     */
    [[nodiscard]] glm::vec2 position() const {
        return {x, y};
    }
};

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_MAPPEDELEMENT_HPP
