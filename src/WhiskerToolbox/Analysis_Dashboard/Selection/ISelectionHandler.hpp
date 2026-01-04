#ifndef ISELECTIONHANDLER_HPP
#define ISELECTIONHANDLER_HPP

#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "SelectionModes.hpp"

#include <QVector2D>

#include <functional>
#include <memory>

class QMouseEvent;
class QKeyEvent;

/**
 * @brief Interface for selection handlers
 * 
 * This interface provides a unified API for all selection handlers in the
 * Analysis Dashboard. It mirrors the IGlyphInteractionController interface
 * from CorePlotting, enabling simplified widget code without std::visit.
 * 
 * Selection handlers implement different selection modes:
 * - LineSelectionHandler: Line intersection selection
 * - PolygonSelectionHandler: Polygon area selection
 * - PointSelectionHandler: Individual point selection
 * - NoneSelectionHandler: No-op (no selection active)
 * 
 * All handlers provide a getPreview() method that returns a GlyphPreview
 * for unified rendering via PreviewRenderer.
 * 
 * @see CorePlotting::Interaction::IGlyphInteractionController
 * @see PlottingOpenGL::PreviewRenderer
 */
class ISelectionHandler {
public:
    using NotificationCallback = std::function<void()>;

    virtual ~ISelectionHandler() = default;

    // Prevent copying (handlers manage state and potentially OpenGL resources)
    ISelectionHandler(ISelectionHandler const &) = delete;
    ISelectionHandler & operator=(ISelectionHandler const &) = delete;
    ISelectionHandler(ISelectionHandler &&) = default;
    ISelectionHandler & operator=(ISelectionHandler &&) = default;

    /**
     * @brief Set the notification callback to be called when selection is completed
     * @param callback The callback function to call when selection is completed
     */
    virtual void setNotificationCallback(NotificationCallback callback) = 0;

    /**
     * @brief Clear the notification callback
     */
    virtual void clearNotificationCallback() = 0;

    // ========================================================================
    // Event Handling
    // ========================================================================

    /**
     * @brief Handle mouse press event
     * @param event The mouse event
     * @param world_pos The mouse position in world coordinates
     */
    virtual void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) = 0;

    /**
     * @brief Handle mouse move event
     * @param event The mouse event
     * @param world_pos The mouse position in world coordinates
     */
    virtual void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) = 0;

    /**
     * @brief Handle mouse release event
     * @param event The mouse event
     * @param world_pos The mouse position in world coordinates
     */
    virtual void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) = 0;

    /**
     * @brief Handle key press event
     * @param event The key event
     */
    virtual void keyPressEvent(QKeyEvent * event) = 0;

    // ========================================================================
    // State
    // ========================================================================

    /**
     * @brief Check if selection is currently active
     * @return true if actively selecting (e.g., drawing line or polygon)
     */
    [[nodiscard]] virtual bool isActive() const = 0;

    /**
     * @brief Deactivate the current selection (cancel any in-progress operation)
     */
    virtual void deactivate() = 0;

    // ========================================================================
    // Preview (for rendering via PreviewRenderer)
    // ========================================================================

    /**
     * @brief Get preview geometry for rendering via PreviewRenderer
     * 
     * This method returns the current selection geometry in canvas coordinates.
     * The widget should pass this to PreviewRenderer::render().
     * 
     * @return GlyphPreview containing the selection geometry, or invalid preview if none
     */
    [[nodiscard]] virtual CorePlotting::Interaction::GlyphPreview getPreview() const = 0;

    // ========================================================================
    // Result
    // ========================================================================

    /**
     * @brief Get the current active selection region (if any)
     * @return Pointer to selection region, or nullptr if none active
     */
    [[nodiscard]] virtual std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const = 0;

protected:
    // Protected default constructor to allow derived classes to construct
    ISelectionHandler() = default;
};

#endif // ISELECTIONHANDLER_HPP
