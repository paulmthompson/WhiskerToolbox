#ifndef POINTSELECTIONHANDLER_HPP
#define POINTSELECTIONHANDLER_HPP

#include "CoreGeometry/points.hpp"
#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "ISelectionHandler.hpp"
#include "SelectionModes.hpp"

#include <QMouseEvent>
#include <QVector2D>

#include <functional>
#include <memory>

class QKeyEvent;


/**
 * @brief Handles point selection functionality for spatial overlay widgets
 * 
 * This class encapsulates the logic for selecting individual points.
 * Unlike line and polygon handlers, point selection is immediate (on click)
 * and doesn't have a preview state.
 */
class PointSelectionHandler : public ISelectionHandler {
public:
    explicit PointSelectionHandler(float world_tolerance);
    ~PointSelectionHandler() override = default;

    // ISelectionHandler interface implementation
    void setNotificationCallback(NotificationCallback callback) override;
    void clearNotificationCallback() override;
    [[nodiscard]] CorePlotting::Interaction::GlyphPreview getPreview() const override {
        // Point selection doesn't have a preview - selection is immediate
        return CorePlotting::Interaction::GlyphPreview{};
    }
    [[nodiscard]] bool isActive() const override { return false; } // Point selection is immediate, not continuous
    void deactivate() override {}

    [[nodiscard]] std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const override {
        static std::unique_ptr<SelectionRegion> null_region = nullptr;
        return null_region;
    }

    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) override;
    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) override {}
    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) override {}
    void keyPressEvent(QKeyEvent * event) override {}

    // Point-specific accessors
    [[nodiscard]] QVector2D getWorldPos() const { return _world_pos; }
    [[nodiscard]] Qt::KeyboardModifiers getModifiers() const { return _modifiers; }

    // TODO: This tolerance should be updated when the zoom level changes
    [[nodiscard]] float getWorldTolerance() const { return _world_tolerance; }


private:
    NotificationCallback _notification_callback;
    QVector2D _world_pos;
    Qt::KeyboardModifiers _modifiers;
    float _world_tolerance;
};

#endif// POINTSELECTIONHANDLER_HPP