
#include "Media_Window.h"

#include <iostream>

/*

The Media_Window class

*/

Media_Window::Media_Window(std::shared_ptr<DataManager> data_manager, QObject *parent) :
    QGraphicsScene(parent),
    _canvasHeight{480},
    _canvasWidth{640},
    _is_verbose{false},
    _data_manager{data_manager}
{
    _createCanvasForData();
}

void Media_Window::addLine(QPainterPath* path, QPen color) {
    auto linePath = addPath(*path,color);
    _line_paths.append(linePath);
}

void Media_Window::addLineDataToScene(const std::string line_key)
{
    _lines_to_show.insert(line_key);
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

    _plotLineData();
}


//Load media designated by frame_id
//Media frame is loaded. It is then scaled to the
//Canvas size, and the canvas is updated
void Media_Window::_convertNewMediaToQImage()
{
    auto _media = _data_manager->getMediaData();
    auto media_data = _media->getRawData();

    auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat()
                              );

    _canvasImage = unscaled_image.scaled(_canvasWidth,_canvasHeight);
}

QImage::Format Media_Window::_getQImageFormat() {

    auto _media = _data_manager->getMediaData();
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

    auto _media = _data_manager->getMediaData();

    float scale_width = static_cast<float>(_canvasWidth)
                        / static_cast<float>(_media->getWidth());

    return scale_width;
}

float Media_Window::getYAspect() const {

    auto _media = _data_manager->getMediaData();

    float scale_height = static_cast<float>(_canvasHeight)
                         / static_cast<float>(_media->getHeight());

    return scale_height;
}

void Media_Window::_plotLineData()
{
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto xAspect = getXAspect();
    auto yAspect = getYAspect();

    for (const auto& line_key : _lines_to_show)
    {

        std::cout << "Plotting lines from " << line_key << std::endl;

        auto lineData = _data_manager->getLine(line_key)->getLinesAtTime(current_time);

        for (const auto& single_line : lineData) {

            QPainterPath* path = new QPainterPath();

            path->moveTo(QPointF(static_cast<float>(single_line[0].x) * xAspect, static_cast<float>(single_line[0].y) * yAspect));

            for (int i = 1; i < single_line.size(); i++) {
                path->lineTo(QPointF(static_cast<float>(single_line[i].x) * xAspect , static_cast<float>(single_line[i].y) * yAspect));
            }

            auto linePath = addPath(*path, QPen(QColor(Qt::blue)));
            _line_paths.append(linePath);
        }



    }
}
