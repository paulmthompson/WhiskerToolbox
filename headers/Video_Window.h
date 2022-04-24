#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include "qdebug.h"
#include <QGraphicsScene>

class Video_Window : public QGraphicsScene
{
Q_OBJECT
public:
    Video_Window(QObject *parent = 0) : QGraphicsScene(parent) {}

protected:

    void mousePressEvent(QGraphicsSceneMouseEvent *event) {
        qDebug() << "Clicked";
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {

    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) {

    }
};


#endif // VIDEO_WINDOW_H
