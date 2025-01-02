
#include "Media_Window.hpp"

#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "TimeFrame.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QImage>

#include <iostream>
#include <QPainter>

/*

The Media_Window class

*/



Media_Window::Media_Window(std::shared_ptr<DataManager> data_manager, QObject *parent) :
    QGraphicsScene(parent),
    _data_manager{data_manager}
{
    _createCanvasForData();

    _data_manager->addObserver([this]() {
        UpdateCanvas();
    });
}

/**
 * @brief Media_Window::addLineDataToScene
 *
 * If a new key is added to DataManager, it is added to the scene
 * by default. This should be observer
 *
 * @param line_key
 */
void Media_Window::addLineDataToScene(std::string const & line_key)
{
    _line_configs[line_key] = element_config{"#0000FF",1.0};
}

void Media_Window::changeLineColor(std::string const & line_key, std::string const & hex_color)
{
    _line_configs[line_key].hex_color = hex_color;
}

void Media_Window::changeLineAlpha(std::string const & line_key, float const alpha)
{
    _line_configs[line_key].alpha = alpha;
}

void Media_Window::clearLines() {
    for (auto pathItem : _line_paths) {
        removeItem(pathItem);
    }
    for (auto pathItem : _line_paths) {
        delete pathItem;
    }
    _line_paths.clear();
}

void Media_Window::addMaskDataToScene(const std::string& mask_key)
{
     _mask_configs[mask_key] = element_config{"#0000FF",1.0};
}

void Media_Window::changeMaskColor(const std::string& mask_key, std::string const & hex_color)
{
     _mask_configs[mask_key].hex_color = hex_color;
}

void Media_Window::changeMaskAlpha(std::string const & line_key, float const alpha)
{
    _mask_configs[line_key].alpha = alpha;
    UpdateCanvas();
}

void Media_Window::changeMaskAlpha(float const alpha)
{
    for (auto & [mask_key, mask_config] : _mask_configs) {
        mask_config.alpha= alpha;
    }
    UpdateCanvas();
}

void Media_Window::clearMasks()
{
    for (auto maskItem : _masks) {
        removeItem(maskItem);
    }

    for (auto maskItem : _masks) {
        delete maskItem;
    }
    _masks.clear();
}

void Media_Window::addPointDataToScene(const std::string& point_key)
{

    _point_configs[point_key] = element_config{"#0000FF",1.0};
}

void Media_Window::changePointColor(std::string const& point_key, std::string const & hex_color)
{
    _point_configs[point_key].hex_color = hex_color;
}

void Media_Window::clearPoints() {
    for (auto pathItem : _points) {
        removeItem(pathItem);
    }
    for (auto pathItem : _points) {
        delete pathItem;
    }
    _points.clear();
}

void Media_Window::setPointAlpha(std::string const & point_key, float const alpha)
{
    _point_configs[point_key].alpha = alpha;
}

void Media_Window::LoadFrame(int frame_id)
{
    // Get MediaData
    auto media = _data_manager->getData<MediaData>("media");
   media->LoadFrame(frame_id);

    UpdateCanvas();
}

void Media_Window::UpdateCanvas()
{
    clearLines();
    clearPoints();
    clearMasks();

    //_convertNewMediaToQImage();
    auto _media = _data_manager->getData<MediaData>("media");
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto media_data = _media->getProcessedData(current_time);

    auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat()
                                 );

    auto new_image = unscaled_image.scaled(_canvasWidth,_canvasHeight);

    _canvasPixmap->setPixmap(QPixmap::fromImage(new_image));
    _canvasImage = new_image;

    // Check for manual selection with the currently rendered frame;

    _plotLineData();

    _plotMaskData();

    _plotPointData();
}


//Load media designated by frame_id
//Media frame is loaded. It is then scaled to the
//Canvas size, and the canvas is updated
void Media_Window::_convertNewMediaToQImage()
{
    auto _media = _data_manager->getData<MediaData>("media");
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto media_data = _media->getProcessedData(current_time);

    auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat()
                              );

    _canvasImage = unscaled_image.scaled(_canvasWidth,_canvasHeight);
}

QImage::Format Media_Window::_getQImageFormat() {

    auto _media = _data_manager->getData<MediaData>("media");
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
        if (_drawing_mode) {
            auto pos = event->scenePos();
            _drawing_points.clear();
            _drawing_points.push_back(pos);
            _is_drawing = true;
        }
        emit leftClick(event->scenePos().x(),event->scenePos().y());
    } else if (event->button() == Qt::RightButton){

    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}
void Media_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton && _is_drawing) {
        _is_drawing = false;
        emit leftRelease();
    }
    QGraphicsScene::mouseReleaseEvent(event);
}
void Media_Window::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (_is_drawing) {
        auto pos = event->scenePos();
        _drawing_points.push_back(pos);
    }
    QGraphicsScene::mouseMoveEvent(event);

}

float Media_Window::getXAspect() const {

    auto _media = _data_manager->getData<MediaData>("media");

    float scale_width = static_cast<float>(_canvasWidth)
                        / static_cast<float>(_media->getWidth());

    return scale_width;
}

float Media_Window::getYAspect() const {

    auto _media = _data_manager->getData<MediaData>("media");

    float scale_height = static_cast<float>(_canvasHeight)
                         / static_cast<float>(_media->getHeight());

    return scale_height;
}

void Media_Window::_plotLineData()
{
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto xAspect = getXAspect();
    auto yAspect = getYAspect();

    for (auto const & [line_key, _line_config] : _line_configs)
    {
        auto plot_color = _plot_color_with_alpha(_line_config);

        auto lineData = _data_manager->getData<LineData>(line_key)->getLinesAtTime(current_time);

        for (auto const & single_line : lineData) {

            if (single_line.size() == 0) {
                continue;
            }

            QPainterPath path = QPainterPath();

            auto single_line_thres = 10.0;

            path.moveTo(QPointF(static_cast<float>(single_line[0].x) * xAspect, static_cast<float>(single_line[0].y) * yAspect));

            for (int i = 1; i < single_line.size(); i++) {
                auto dx = single_line[i].x - single_line[i-1].x;
                auto dy = single_line[i].y - single_line[i-1].y;
                auto d = std::sqrt((dx * dx) + (dy * dy));
                if (d > single_line_thres) {
                    path.moveTo(QPointF(static_cast<float>(single_line[i].x) * xAspect , static_cast<float>(single_line[i].y) * yAspect));
                } else {
                    path.lineTo(QPointF(static_cast<float>(single_line[i].x) * xAspect , static_cast<float>(single_line[i].y) * yAspect));
                }
            }

            auto linePath = addPath(path, QPen(plot_color));
            _line_paths.append(linePath);

            /*
            // Add dots for each point on the line
            for (const auto & point : single_line) {
                auto ellipse = addEllipse(
                        static_cast<float>(point.x) * xAspect - 2.5,
                        static_cast<float>(point.y) * yAspect - 2.5,
                        5.0, 5.0,
                        QPen(plot_color),
                        QBrush(plot_color)
                );
                _points.append(ellipse);
            }
             */
        }

    }
}

QRgb Media_Window::_plot_color_with_alpha(element_config elem)
{
    auto color = QColor(QString::fromStdString(elem.hex_color));
    auto output_color = qRgba(color.red(), color.green(), color.blue(), elem.alpha * 255);

    return output_color;
}

void Media_Window::_plotMaskData()
{
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (auto const & [mask_key, _mask_config] : _mask_configs)
    {
        auto plot_color = _plot_color_with_alpha(_mask_config);

        auto mask = _data_manager->getData<MaskData>(mask_key);
        float mask_height = static_cast<float>(mask->getMaskHeight());
        float mask_width = static_cast<float>(mask->getMaskWidth());

        auto const& maskData = mask->getMasksAtTime(current_time);

        _plotSingleMaskData(maskData, mask_width, mask_height, plot_color);

        auto const& maskData2 = mask->getMasksAtTime(-1);

        _plotSingleMaskData(maskData2, mask_width, mask_height, plot_color);

    }
}

void Media_Window::_plotSingleMaskData(std::vector<Mask2D> const & maskData, int const mask_width, int const mask_height, QRgb plot_color)
{
    for (auto const& single_mask : maskData) {

        QImage unscaled_mask_image(mask_width, mask_height,QImage::Format::Format_ARGB32);

        unscaled_mask_image.fill(0);

        for (int i = 0; i < single_mask.size(); i ++)
        {
            unscaled_mask_image.setPixel(
                QPoint(single_mask[i].x, single_mask[i].y),
                plot_color);
        }

        auto scaled_mask_image = unscaled_mask_image.scaled(_canvasWidth,_canvasHeight);

        auto maskPixmap = addPixmap(QPixmap::fromImage(scaled_mask_image));

        _masks.append(maskPixmap);
    }
}

void Media_Window::_plotPointData()
{
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    int i =0;

    for (auto const & [point_key, _point_config] : _point_configs)
    {

        auto plot_color = _plot_color_with_alpha(_point_config);

        auto point = _data_manager->getData<PointData>(point_key);

        float mask_height = static_cast<float>(point->getMaskHeight());
        float mask_width = static_cast<float>(point->getMaskWidth());

        auto xAspect = _canvasWidth / mask_width;
        auto yAspect = _canvasHeight / mask_height;

        auto pointData = point->getPointsAtTime(current_time);


        auto pen = QPen(plot_color);
        pen.setWidth(3);
        for (const auto& single_point : pointData) {
            auto ellipse = addEllipse(single_point.y * xAspect, single_point.x * yAspect, 10.0, 10.0, pen);
            _points.append(ellipse);
        }
        i ++;
    }
}

std::vector<uint8_t> Media_Window::getDrawingMask() {
    // Create a QImage with _canvasWidth and _canvasHeight
    QImage maskImage(_canvasWidth, _canvasHeight, QImage::Format_Grayscale8);
    maskImage.fill(0);

    QPainter painter(&maskImage);
    painter.setPen(Qt::white);

    for (const auto& point : _drawing_points) {
        painter.drawPoint(point.toPoint());
    }
    painter.end();

    // Scale the QImage to the size of the media
    auto media = _data_manager->getData<MediaData>("media");
    int mediaWidth = media->getWidth();
    int mediaHeight = media->getHeight();
    QImage scaledMaskImage = maskImage.scaled(mediaWidth, mediaHeight);

    // Convert the QImage to a std::vector<uint8_t>
    std::vector<uint8_t> mask(scaledMaskImage.bits(), scaledMaskImage.bits() + scaledMaskImage.sizeInBytes());

    return mask;
}

