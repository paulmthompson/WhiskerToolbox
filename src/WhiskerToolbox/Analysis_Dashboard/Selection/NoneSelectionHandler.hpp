#ifndef NONESELECTIONHANDLER_HPP
#define NONESELECTIONHANDLER_HPP

#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "ISelectionHandler.hpp"
#include "SelectionModes.hpp"

#include <QVector2D>

#include <functional>
#include <memory>

class QKeyEvent;
class QMouseEvent;

/**
 * @brief No-op selection handler for when no selection mode is active
 * 
 * This class provides a default implementation that does nothing,
 * simplifying the handler management in the main widget.
 */
class NoneSelectionHandler : public ISelectionHandler {
public:
    explicit NoneSelectionHandler() = default;
    ~NoneSelectionHandler() override = default;

    // ISelectionHandler interface implementation (all no-ops)
    void setNotificationCallback(NotificationCallback callback) override {
        // No-op - this handler doesn't do anything
    }

    void clearNotificationCallback() override {
        // No-op - this handler doesn't do anything
    }

    [[nodiscard]] CorePlotting::Interaction::GlyphPreview getPreview() const override {
        // No-op - this handler doesn't render anything
        return CorePlotting::Interaction::GlyphPreview{};
    }

    [[nodiscard]] bool isActive() const override {
        return false; // Never active
    }

    void deactivate() override {
        // No-op - this handler doesn't have any state to deactivate
    }

    [[nodiscard]] std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const override {
        static std::unique_ptr<SelectionRegion> null_region = nullptr;
        return null_region;
    }

    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) override {
        // No-op - this handler doesn't respond to mouse events
    }

    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) override {
        // No-op
    }

    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) override {
        // No-op
    }

    void keyPressEvent(QKeyEvent * event) override {
        // No-op - this handler doesn't respond to key events
    }
};

#endif// NONESELECTIONHANDLER_HPP