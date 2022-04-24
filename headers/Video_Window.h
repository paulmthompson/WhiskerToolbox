#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include "qdebug.h"
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

class Video_Window : public QGraphicsScene
{
Q_OBJECT
public:
    Video_Window(QObject *parent = 0) : QGraphicsScene(parent) {}

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            emit leftClick(event->scenePos().x(),event->scenePos().y());
        } else if (event->button() == Qt::RightButton){

        }
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {

    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) {

    }
signals:
    void leftClick(qreal,qreal);
};


#endif // VIDEO_WINDOW_H
