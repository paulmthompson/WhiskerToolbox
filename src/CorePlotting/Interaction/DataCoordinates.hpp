#ifndef COREPLOTTING_INTERACTION_DATACOORDINATES_HPP
#define COREPLOTTING_INTERACTION_DATACOORDINATES_HPP

#include "Entity/EntityTypes.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace CorePlotting::Interaction {

/**
 * @brief Result of converting preview geometry to data-space coordinates
 * 
 * This struct is the output of RenderableScene::previewToDataCoords() and
 * contains all the information needed to commit an interaction result to
 * the DataManager.
 * 
 * **Coordinate Spaces**:
 * - GlyphPreview: Canvas coordinates (pixels)
 * - DataCoordinates: Data-space coordinates (time indices, data values)
 * 
 * **Usage**:
 * After an interaction completes, the widget:
 * 1. Gets the GlyphPreview from the controller
 * 2. Calls scene.previewToDataCoords() to convert to data space
 * 3. Uses this struct to update the DataManager
 * 
 * @see GlyphPreview for the input format
 * @see RenderableScene::previewToDataCoords() for the conversion
 */
struct DataCoordinates {
    /// Identifier of the target series
    std::string series_key;
    
    /// EntityId if modifying an existing element, std::nullopt if creating new
    std::optional<EntityId> entity_id;
    
    /// true if modifying existing element, false if creating new
    bool is_modification{false};

    // ========================================================================
    // Type-Specific Data Coordinates
    // ========================================================================

    /**
     * @brief Interval coordinates (for DigitalIntervalSeries)
     * 
     * Times are in TimeFrameIndex units (integer indices into TimeFrame).
     * Suitable for direct use with DigitalIntervalSeries::addEvent().
     */
    struct IntervalCoords {
        int64_t start{0};  ///< Start time (inclusive)
        int64_t end{0};    ///< End time (inclusive)
        
        [[nodiscard]] bool isValid() const { return start <= end; }
        [[nodiscard]] int64_t duration() const { return end - start; }
    };

    /**
     * @brief Line coordinates (for line annotations or selections)
     * 
     * All values are in data space (world X = time, world Y = data value).
     */
    struct LineCoords {
        float x1{0}, y1{0};  ///< Start point
        float x2{0}, y2{0};  ///< End point
        
        [[nodiscard]] float length() const {
            float const dx = x2 - x1;
            float const dy = y2 - y1;
            return std::sqrt(dx * dx + dy * dy);
        }
    };

    /**
     * @brief Point coordinates (for point placement)
     */
    struct PointCoords {
        float x{0};  ///< X coordinate (time or spatial X)
        float y{0};  ///< Y coordinate (data value or spatial Y)
    };

    /**
     * @brief Rectangle coordinates (for selection boxes)
     * 
     * Origin is at (x, y), extending by (width, height).
     */
    struct RectCoords {
        float x{0};       ///< Left edge X
        float y{0};       ///< Bottom edge Y
        float width{0};   ///< Width (positive)
        float height{0};  ///< Height (positive)
        
        [[nodiscard]] float right() const { return x + width; }
        [[nodiscard]] float top() const { return y + height; }
        [[nodiscard]] bool isValid() const { return width >= 0 && height >= 0; }
    };

    /// The actual coordinates (type depends on interaction type)
    std::variant<std::monostate, IntervalCoords, LineCoords, PointCoords, RectCoords> coords;

    // ========================================================================
    // Type Query Methods
    // ========================================================================

    /**
     * @brief Check if coordinates have been set
     */
    [[nodiscard]] bool hasCoords() const {
        return !std::holds_alternative<std::monostate>(coords);
    }

    /**
     * @brief Check if this contains interval coordinates
     */
    [[nodiscard]] bool isInterval() const {
        return std::holds_alternative<IntervalCoords>(coords);
    }

    /**
     * @brief Check if this contains line coordinates
     */
    [[nodiscard]] bool isLine() const {
        return std::holds_alternative<LineCoords>(coords);
    }

    /**
     * @brief Check if this contains point coordinates
     */
    [[nodiscard]] bool isPoint() const {
        return std::holds_alternative<PointCoords>(coords);
    }

    /**
     * @brief Check if this contains rectangle coordinates
     */
    [[nodiscard]] bool isRect() const {
        return std::holds_alternative<RectCoords>(coords);
    }

    // ========================================================================
    // Type-Safe Accessors
    // ========================================================================

    /**
     * @brief Get interval coordinates
     * @throws std::bad_variant_access if not an interval
     */
    [[nodiscard]] IntervalCoords const& asInterval() const {
        return std::get<IntervalCoords>(coords);
    }

    /**
     * @brief Get line coordinates
     * @throws std::bad_variant_access if not a line
     */
    [[nodiscard]] LineCoords const& asLine() const {
        return std::get<LineCoords>(coords);
    }

    /**
     * @brief Get point coordinates
     * @throws std::bad_variant_access if not a point
     */
    [[nodiscard]] PointCoords const& asPoint() const {
        return std::get<PointCoords>(coords);
    }

    /**
     * @brief Get rectangle coordinates
     * @throws std::bad_variant_access if not a rectangle
     */
    [[nodiscard]] RectCoords const& asRect() const {
        return std::get<RectCoords>(coords);
    }

    // ========================================================================
    // Safe Accessors (return std::optional)
    // ========================================================================

    [[nodiscard]] std::optional<IntervalCoords> tryAsInterval() const {
        if (auto const* p = std::get_if<IntervalCoords>(&coords)) {
            return *p;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<LineCoords> tryAsLine() const {
        if (auto const* p = std::get_if<LineCoords>(&coords)) {
            return *p;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<PointCoords> tryAsPoint() const {
        if (auto const* p = std::get_if<PointCoords>(&coords)) {
            return *p;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<RectCoords> tryAsRect() const {
        if (auto const* p = std::get_if<RectCoords>(&coords)) {
            return *p;
        }
        return std::nullopt;
    }

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create DataCoordinates for a new interval
     */
    [[nodiscard]] static DataCoordinates createInterval(
            std::string series_key,
            int64_t start,
            int64_t end) {
        DataCoordinates result;
        result.series_key = std::move(series_key);
        result.is_modification = false;
        result.coords = IntervalCoords{start, end};
        return result;
    }

    /**
     * @brief Create DataCoordinates for modifying an existing interval
     */
    [[nodiscard]] static DataCoordinates modifyInterval(
            std::string series_key,
            EntityId entity_id,
            int64_t new_start,
            int64_t new_end) {
        DataCoordinates result;
        result.series_key = std::move(series_key);
        result.entity_id = entity_id;
        result.is_modification = true;
        result.coords = IntervalCoords{new_start, new_end};
        return result;
    }

    /**
     * @brief Create DataCoordinates for a new line
     */
    [[nodiscard]] static DataCoordinates createLine(
            std::string series_key,
            float x1, float y1,
            float x2, float y2) {
        DataCoordinates result;
        result.series_key = std::move(series_key);
        result.is_modification = false;
        result.coords = LineCoords{x1, y1, x2, y2};
        return result;
    }

    /**
     * @brief Create DataCoordinates for a new point
     */
    [[nodiscard]] static DataCoordinates createPoint(
            std::string series_key,
            float x, float y) {
        DataCoordinates result;
        result.series_key = std::move(series_key);
        result.is_modification = false;
        result.coords = PointCoords{x, y};
        return result;
    }

    /**
     * @brief Create DataCoordinates for a new rectangle
     */
    [[nodiscard]] static DataCoordinates createRect(
            std::string series_key,
            float x, float y,
            float width, float height) {
        DataCoordinates result;
        result.series_key = std::move(series_key);
        result.is_modification = false;
        result.coords = RectCoords{x, y, width, height};
        return result;
    }
};

} // namespace CorePlotting::Interaction

#endif // COREPLOTTING_INTERACTION_DATACOORDINATES_HPP
