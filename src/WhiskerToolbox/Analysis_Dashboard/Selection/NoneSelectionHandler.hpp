#ifndef NONESELECTIONHANDLER_HPP
#define NONESELECTIONHANDLER_HPP

#include "SelectionModes.hpp"

#include <QMatrix4x4>
#include <QVector2D>

#include <functional>
#include <memory>

class QKeyEvent;
class QMouseEvent;

/**
 * @brief No-op selection handler for when no selection mode is active
 * 
 * This class provides a default implementation that does nothing,
 * simplifying the variant handling in the main widget.
 */
class NoneSelectionHandler {
public:
    using NotificationCallback = std::function<void()>;

    explicit NoneSelectionHandler() = default;
    ~NoneSelectionHandler() = default;

    /**
     * @brief Set the notification callback (no-op)
     * @param callback The callback function (ignored)
     */
    void setNotificationCallback(NotificationCallback callback) {
        // No-op - this handler doesn't do anything
    }

    /**
     * @brief Clear the notification callback (no-op)
     */
    void clearNotificationCallback() {
        // No-op - this handler doesn't do anything
    }

    /**
     * @brief Render nothing (no-op)
     * @param mvp_matrix Model-View-Projection matrix (ignored)
     */
    void render(QMatrix4x4 const & mvp_matrix) {
        // No-op - this handler doesn't render anything
    }

    /**
     * @brief Deactivate (no-op)
     */
    void deactivate() {
        // No-op - this handler doesn't have any state to deactivate
    }

    /**
     * @brief Get the current active selection region (always nullptr)
     * @return nullptr
     */
    std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const {
        static std::unique_ptr<SelectionRegion> null_region = nullptr;
        return null_region;
    }

    /**
     * @brief Handle mouse press events (no-op)
     * @param event Mouse event (ignored)
     * @param world_pos World position (ignored)
     */
    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) {
        // No-op - this handler doesn't respond to mouse events
    }

    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) {
    }

    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) {}

    /**
     * @brief Handle key press events (no-op)
     * @param event Key event (ignored)
     */
    void keyPressEvent(QKeyEvent * event) {
        // No-op - this handler doesn't respond to key events
    }
};

#endif// NONESELECTIONHANDLER_HPP