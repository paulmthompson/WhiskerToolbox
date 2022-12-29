#ifndef VIDEO_WINDOW_H
#define VIDEO_WINDOW_H

#include "qdebug.h"
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>

class Video_Window : public QGraphicsScene
{
Q_OBJECT
public:
    Video_Window(QObject *parent = 0) : QGraphicsScene(parent) {
        this->myimage = QImage(640,480,QImage::Format_Grayscale8);
        this->pixmap_item = addPixmap(QPixmap::fromImage(this->myimage));
    }

    void addLine(QPainterPath* path, QPen color) {
        line_paths.append(addPath(*path,color));
    }

    void clearLines() {
        for (auto pathItem : this->line_paths) {
            removeItem(pathItem);
        }
        this->line_paths.clear();
    }

    void UpdateCanvas(QImage& img)
    {
        clearLines();
        this->pixmap_item->setPixmap(QPixmap::fromImage(img));
    }


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


    QVector<QGraphicsPathItem*> line_paths;
    QGraphicsPixmapItem* pixmap_item;
    QImage myimage;


signals:
    void leftClick(qreal,qreal);
};


#endif // VIDEO_WINDOW_H
