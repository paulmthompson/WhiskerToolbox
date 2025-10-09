#ifndef MEDIAPOINT_WIDGET_HPP
#define MEDIAPOINT_WIDGET_HPP

#include "Entity/EntityTypes.hpp"

#include <QWidget>

#include <string>

namespace Ui {
class MediaPoint_Widget;
}

class DataManager;
class Media_Window;
class QGraphicsSceneMouseEvent;

class MediaPoint_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaPoint_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaPoint_Widget() override;
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;

    void setActiveKey(std::string const & key);

private:
    Ui::MediaPoint_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;
    bool _selection_enabled = false;
    
    // Point selection state
    EntityId _selected_point_id = 0;
    float _selection_threshold = 10.0f; // pixels
    
    // Helper methods for point interaction
    EntityId _findNearestPoint(qreal x_media, qreal y_media, float max_distance);
    void _selectPoint(EntityId point_id);
    void _clearPointSelection();
    void _moveSelectedPoint(qreal x_media, qreal y_media);
    void _assignPoint(qreal x_media, qreal y_media);
    void _addPointAtCurrentTime(qreal x_media, qreal y_media);

private slots:
    //void _handlePointClick(qreal x_media, qreal y_media);
    void _handlePointClickWithModifiers(qreal x_media, qreal y_media, Qt::KeyboardModifiers modifiers);
    void _setPointColor(const QString& hex_color);
    void _setPointAlpha(int alpha);
    void _setPointSize(int size);
    void _setMarkerShape(int shapeIndex);

};

#endif// MEDIAPOINT_WIDGET_HPP
