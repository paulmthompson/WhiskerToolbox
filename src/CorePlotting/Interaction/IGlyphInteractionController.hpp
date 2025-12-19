#ifndef COREPLOTTING_INTERACTION_IGLYPHINTERACTIONCONTROLLER_HPP
#define COREPLOTTING_INTERACTION_IGLYPHINTERACTIONCONTROLLER_HPP

#include "GlyphPreview.hpp"
#include "Entity/EntityTypes.hpp"

#include <optional>
#include <string>

namespace CorePlotting::Interaction {

/**
 * @brief Abstract interface for interactive glyph creation and modification controllers
 * 
 * This interface defines the contract for controllers that handle user interactions
 * for creating or modifying visual elements (rectangles, lines, points, polygons).
 * 
 * **Coordinate System**: All coordinates are in canvas pixels (screen space).
 * The widget is responsible for passing canvas coordinates from mouse events,
 * and the Scene handles conversion to world/data coordinates via inverse transforms.
 * 
 * **Lifecycle**:
 * 1. Widget creates appropriate controller based on interaction mode
 * 2. `start()` called on mouse press with canvas coordinates
 * 3. `update()` called on mouse move to update preview
 * 4. `complete()` called on mouse release to finalize
 * 5. Widget retrieves preview via `getPreview()` and converts to data coordinates
 * 6. Controller can be reused or destroyed
 * 
 * **Modification vs Creation**:
 * - Creating new: `start()` with `existing_entity_id = std::nullopt`
 * - Modifying existing: `start()` with the EntityId of the element being modified
 *   (e.g., interval edge drag). The controller may need additional info passed
 *   via a subclass-specific method (see RectangleInteractionController::startEdgeDrag).
 * 
 * @see GlyphPreview for the preview geometry format
 * @see RenderableScene::previewToDataCoords() for coordinate conversion
 */
class IGlyphInteractionController {
public:
    virtual ~IGlyphInteractionController() = default;

    // ========================================================================
    // Lifecycle Methods
    // ========================================================================

    /**
     * @brief Begin a new interaction
     * 
     * Called when the user initiates an interaction (typically on mouse press).
     * 
     * @param canvas_x X coordinate in canvas pixels
     * @param canvas_y Y coordinate in canvas pixels
     * @param series_key Identifier of the target series (for data commit)
     * @param existing_entity_id If modifying an existing element, its EntityId;
     *                           std::nullopt if creating a new element
     */
    virtual void start(float canvas_x, float canvas_y,
                       std::string series_key,
                       std::optional<EntityId> existing_entity_id = std::nullopt) = 0;

    /**
     * @brief Update the interaction with new cursor position
     * 
     * Called during mouse move while the interaction is active.
     * Updates the preview geometry based on the new position.
     * 
     * @param canvas_x Current X coordinate in canvas pixels
     * @param canvas_y Current Y coordinate in canvas pixels
     */
    virtual void update(float canvas_x, float canvas_y) = 0;

    /**
     * @brief Complete the interaction successfully
     * 
     * Called when the user finishes the interaction (typically on mouse release).
     * After this call, `isActive()` returns false and `getPreview()` contains
     * the final geometry for conversion to data coordinates.
     */
    virtual void complete() = 0;

    /**
     * @brief Cancel the interaction without applying changes
     * 
     * Called when the interaction should be aborted (e.g., Escape key pressed).
     * After this call, `isActive()` returns false.
     */
    virtual void cancel() = 0;

    // ========================================================================
    // State Query Methods
    // ========================================================================

    /**
     * @brief Check if an interaction is currently in progress
     * @return true if between start() and complete()/cancel()
     */
    [[nodiscard]] virtual bool isActive() const = 0;

    /**
     * @brief Get the current preview geometry
     * 
     * Returns the preview in canvas coordinates, suitable for rendering
     * via PreviewRenderer. The preview type matches the controller type
     * (e.g., RectangleInteractionController produces Rectangle previews).
     * 
     * @return Current preview geometry (valid even after complete())
     */
    [[nodiscard]] virtual GlyphPreview getPreview() const = 0;

    /**
     * @brief Get the target series key
     * @return Series key passed to start(), empty string if not active
     */
    [[nodiscard]] virtual std::string const& getSeriesKey() const = 0;

    /**
     * @brief Get the EntityId if modifying an existing element
     * @return EntityId if modifying, std::nullopt if creating new
     */
    [[nodiscard]] virtual std::optional<EntityId> getEntityId() const = 0;

    /**
     * @brief Check if this interaction is modifying an existing element
     * @return true if modifying existing (has EntityId), false if creating new
     */
    [[nodiscard]] bool isModification() const {
        return getEntityId().has_value();
    }
};

} // namespace CorePlotting::Interaction

#endif // COREPLOTTING_INTERACTION_IGLYPHINTERACTIONCONTROLLER_HPP
