
#include "Media_Window.h"

#include <iostream>

/*

The Media_Window class

*/

Media_Window::Media_Window(QObject *parent) :
    QGraphicsScene(parent),
    _media{std::make_shared<MediaData>()},
    _canvasHeight{480},
    _canvasWidth{640},
    _is_verbose{false}
{
    _createCanvasForData();
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

void Media_Window::UpdateCanvas()
{
    clearLines();
    clearPoints();

    _convertNewMediaToQImage();

    _canvasPixmap->setPixmap(QPixmap::fromImage(_canvasImage));
}


//Load media designated by frame_id
//Media frame is loaded. It is then scaled to the
//Canvas size, and the canvas is updated
void Media_Window::_convertNewMediaToQImage()
{
    auto media_data = _media->getRawData();

    auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat()
                              );

    _canvasImage = unscaled_image.scaled(_canvasWidth,_canvasHeight);
}

QImage::Format Media_Window::_getQImageFormat() {

    switch(_media->getFormat())
    {
    case MediaData::DisplayFormat::Gray:
        return QImage::Format_Grayscale8;
    case MediaData::DisplayFormat::Color:
        return QImage::Format_RGBA8888;
    }
}

void Media_Window::_createCanvasForData()
{

    auto image_format = _getQImageFormat();

    _mediaImage = QImage(_canvasWidth,_canvasHeight,image_format);
    _canvasImage = QImage(_canvasWidth,_canvasHeight,image_format);

    _canvasPixmap = addPixmap(QPixmap::fromImage(_canvasImage));
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
                        / static_cast<float>(_media->getWidth());

    return scale_width;
}

float Media_Window::getYAspect() const {

    float scale_height = static_cast<float>(_canvasHeight)
                         / static_cast<float>(_media->getHeight());

    return scale_height;
}
