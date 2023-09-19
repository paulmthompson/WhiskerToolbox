#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>

#include "Media_Window.h"

#include <iostream>

/*

The Media_Window class

*/

Media_Window::Media_Window(QObject *parent) : QGraphicsScene(parent) {

    canvasWidth = 640;
    canvasHeight = 480;
    this->canvasImage = QImage(canvasWidth,canvasHeight,QImage::Format_Grayscale8);
    this->canvasPixmap = addPixmap(QPixmap::fromImage(this->canvasImage));

    total_frame_count = 0; // This should be in data / time object

    verbose_frame = false;
}

void Media_Window::addLine(QPainterPath* path, QPen color) {
    auto linePath = addPath(*path,color);
    this->line_paths.append(linePath);
}

void Media_Window::clearLines() {
    for (auto pathItem : this->line_paths) {
        removeItem(pathItem);
    }
    this->line_paths.clear();
}

void Media_Window::clearPoints() {
    for (auto pathItem : this->points) {
        removeItem(pathItem);
    }
    this->points.clear();
}

void Media_Window::UpdateCanvas(QImage& img)
{
    clearLines();
    clearPoints();

    //We should check size of image here to ensure that its the correct size
    this->canvasPixmap->setPixmap(QPixmap::fromImage(img));
}

int Media_Window::LoadMedia(std::string name) {

    this->total_frame_count = this->doLoadMedia(name);
    return this->total_frame_count;
}

//Load media designated by frame_id
//Media frame is loaded. It is then scaled to the
//Canvas size, and the canvas is updated
int Media_Window::LoadFrame(int frame_id)
{

    //frame_id = this->checkFrameInbounds(frame_id);

    doLoadFrame(frame_id);

    this->canvasImage = mediaImage.scaled(this->canvasWidth,this->canvasHeight);

    UpdateCanvas(this->canvasImage);

    return frame_id;

    //this->last_loaded_frame = frame_id;
    //return this->last_loaded_frame;
}

void Media_Window::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit leftClick(event->scenePos().x(),event->scenePos().y());
    } else if (event->button() == Qt::RightButton){

    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}
void Media_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseReleaseEvent(event);
}
void Media_Window::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsScene::mouseMoveEvent(event);

}

float Media_Window::getXAspect() const {

    float scale_width = static_cast<float>(this->canvasWidth)
                        / static_cast<float>(this->mediaWidth);

    return scale_width;
}

float Media_Window::getYAspect() const {

    float scale_height = static_cast<float>(this->canvasHeight)
                         / static_cast<float>(this->mediaHeight);

    return scale_height;
}
