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

    _canvasWidth = 640;
    _canvasHeight = 480;

    _mediaImage = QImage(_canvasWidth,_canvasHeight,QImage::Format_Grayscale8);
    _canvasImage = QImage(_canvasWidth,_canvasHeight,QImage::Format_Grayscale8);
    _canvasPixmap = addPixmap(QPixmap::fromImage(_canvasImage));

    this->media = std::make_shared<MediaData>();

    _is_verbose = false;
}

void Media_Window::addLine(QPainterPath* path, QPen color) {
    auto linePath = addPath(*path,color);
    _line_paths.append(linePath);
}

void Media_Window::clearLines() {
    for (auto pathItem : _line_paths) {
        removeItem(pathItem);
    }
    _line_paths.clear();
}

void Media_Window::clearPoints() {
    for (auto pathItem : _points) {
        removeItem(pathItem);
    }
    _points.clear();
}

void Media_Window::UpdateCanvas(QImage& img)
{
    clearLines();
    clearPoints();

    //We should check size of image here to ensure that its the correct size
    _canvasPixmap->setPixmap(QPixmap::fromImage(img));
}

int Media_Window::LoadMedia(std::string name) {

    auto total_frame_count = this->media->LoadMedia(name);
    this->media->setTotalFrameCount(total_frame_count);
    return total_frame_count;
}

//Load media designated by frame_id
//Media frame is loaded. It is then scaled to the
//Canvas size, and the canvas is updated
int Media_Window::LoadFrame(int frame_id)
{
    // Get MediaData
    this->media->LoadFrame(frame_id);

    auto media_data = this->media->getData();

    //std::cout << this->media->getHeight() << " x " << this->media->getWidth() << std::endl;
    auto unscaled_image = QImage(&media_data[0],
                              this->media->getWidth(),
                              this->media->getHeight(),
                              QImage::Format(this->media->getFormat())
                              );

    //std::cout << unscaled_image.height() << " x " << unscaled_image.width() << std::endl;

    _canvasImage = unscaled_image.scaled(_canvasWidth,_canvasHeight);

    UpdateCanvas(_canvasImage);

    return frame_id;
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

    float scale_width = static_cast<float>(_canvasWidth)
                        / static_cast<float>(this->media->getWidth());

    return scale_width;
}

float Media_Window::getYAspect() const {

    float scale_height = static_cast<float>(_canvasHeight)
                         / static_cast<float>(this->media->getHeight());

    return scale_height;
}
