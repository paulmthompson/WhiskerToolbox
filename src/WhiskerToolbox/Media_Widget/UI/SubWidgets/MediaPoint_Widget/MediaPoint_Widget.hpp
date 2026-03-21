#ifndef MEDIAPOINT_WIDGET_HPP
#define MEDIAPOINT_WIDGET_HPP

/**
 * @file MediaPoint_Widget.hpp
 * @brief Widget for point data display options and interaction in Media_Widget
 */

#include "Entity/EntityTypes.hpp"

#include <QWidget>

#include <string>

namespace Ui {
class MediaPoint_Widget;
}

class DataManager;
class GlyphStyleControls;
class GlyphStyleState;
class Media_Window;
class QGraphicsSceneMouseEvent;
class MediaWidgetState;

class MediaPoint_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaPoint_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state = nullptr, QWidget * parent = nullptr);
    ~MediaPoint_Widget() override;
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;

    void setActiveKey(std::string const & key);

private:
    Ui::MediaPoint_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    MediaWidgetState * _state{nullptr};
    std::string _active_key;
    bool _selection_enabled = false;

    // Glyph style controls (replaces bespoke color/alpha/size/shape controls)
    GlyphStyleState * _glyph_state{nullptr};
    GlyphStyleControls * _glyph_controls{nullptr};

    // Point selection state
    EntityId _selected_point_id = EntityId(0);
    float _selection_threshold = 10.0f;// pixels

    // Helper methods for point interaction
    EntityId _findNearestPoint(qreal x_media, qreal y_media, float max_distance);
    void _selectPoint(EntityId point_id);
    void _clearPointSelection();
    void _moveSelectedPoint(qreal x_media, qreal y_media);
    void _assignPoint(qreal x_media, qreal y_media);
    void _addPointAtCurrentTime(qreal x_media, qreal y_media);

    /// @brief Sync GlyphStyleState to the current PointDisplayOptions
    void _syncGlyphStateFromOptions();

    /// @brief Apply GlyphStyleState changes back to PointDisplayOptions
    void _applyGlyphStateToOptions();

private slots:
    void _handlePointClickWithModifiers(qreal x_media, qreal y_media, Qt::KeyboardModifiers modifiers);
};

#endif// MEDIAPOINT_WIDGET_HPP
