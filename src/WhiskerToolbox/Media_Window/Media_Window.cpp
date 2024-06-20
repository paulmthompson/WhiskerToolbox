
#include "Media_Window.hpp"

#include "Media/Media_Data.hpp"

/*

The Media_Window class

*/



Media_Window::Media_Window(std::shared_ptr<DataManager> data_manager, QObject *parent) :
    QGraphicsScene(parent),
    _data_manager{data_manager},
    _line_colors{}
{
    _createCanvasForData();
}

void Media_Window::addLineDataToScene(const std::string line_key)
{
    _lines_to_show.insert(line_key);
}

void Media_Window::addLineColor(std::string line_key, const QColor color)
{
    _line_colors[line_key] = color;
}

void Media_Window::clearLines() {
    for (auto pathItem : _line_paths) {
        removeItem(pathItem);
    }
    _line_paths.clear();
}

void Media_Window::addMaskDataToScene(const std::string& mask_key)
{
     _masks_to_show.insert(mask_key);
}

void Media_Window::addMaskColor(const std::string& mask_key, const QColor color )
{
     _mask_colors[mask_key] = color;
}

void Media_Window::clearMasks()
{
    for (auto maskItem : _masks) {
        removeItem(maskItem);
    }
    _masks.clear();
}

void Media_Window::addPointDataToScene(const std::string& point_key)
{
    _points_to_show.insert(point_key);
}

void Media_Window::addPointColor(std::string const& point_key, QColor const color)
{
    _point_colors[point_key] = color;
}

void Media_Window::clearPoints() {
    for (auto pathItem : _points) {
        removeItem(pathItem);
    }
    _points.clear();
}

void Media_Window::LoadFrame(int frame_id)
{
    // Get MediaData
    _data_manager->getMediaData()->LoadFrame(frame_id);

    UpdateCanvas();
}

void Media_Window::UpdateCanvas()
{
    clearLines();
    clearPoints();
    clearMasks();

    _convertNewMediaToQImage();

    _canvasPixmap->setPixmap(QPixmap::fromImage(_canvasImage));

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
    auto _media = _data_manager->getMediaData();
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

    int i =0;
    for (auto const & line_key : _lines_to_show)
    {

        auto plot_color = QColor("blue");
        if (_line_colors.count(line_key) != 0)
        {
            plot_color = QColor(_line_colors[line_key]);
        }

        auto lineData = _data_manager->getLine(line_key)->getLinesAtTime(current_time);

        for (auto const & single_line : lineData) {

            QPainterPath path = QPainterPath();

            path.moveTo(QPointF(static_cast<float>(single_line[0].x) * xAspect, static_cast<float>(single_line[0].y) * yAspect));

            for (int i = 1; i < single_line.size(); i++) {
                path.lineTo(QPointF(static_cast<float>(single_line[i].x) * xAspect , static_cast<float>(single_line[i].y) * yAspect));
            }

            auto linePath = addPath(path, QPen(plot_color));
            _line_paths.append(linePath);
        }
        i ++;
    }
}

QRgb Media_Window::_create_mask_plot_color(const std::string& mask_key) {

    auto plot_color = QColor("blue");
    if (_mask_colors.count(mask_key) != 0)
    {
        plot_color = QColor(_mask_colors[mask_key]);
    }

    auto output_color = qRgba(plot_color.red(), plot_color.green(), plot_color.blue(), _mask_alpha);

    return output_color;
}

void Media_Window::_plotMaskData()
{
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (auto const& mask_key : _masks_to_show)
    {
        auto plot_color = _create_mask_plot_color(mask_key);

        float mask_height = static_cast<float>(_data_manager->getMask(mask_key)->getMaskHeight());
        float mask_width = static_cast<float>(_data_manager->getMask(mask_key)->getMaskWidth());

        auto const& maskData = _data_manager->getMask(mask_key)->getMasksAtTime(current_time);

        _plotSingleMaskData(maskData, mask_width, mask_height, plot_color);

        auto const& maskData2 = _data_manager->getMask(mask_key)->getMasksAtTime(-1);

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

    for (auto const & point_key : _points_to_show)
    {

        auto plot_color = QColor("blue");
        if (_point_colors.count(point_key) != 0)
        {
            plot_color = QColor(_point_colors[point_key]);
        }

        float mask_height = static_cast<float>(_data_manager->getPoint(point_key)->getMaskHeight());
        float mask_width = static_cast<float>(_data_manager->getPoint(point_key)->getMaskWidth());

        auto xAspect = _canvasWidth / mask_width;
        auto yAspect = _canvasHeight / mask_height;

        auto pointData = _data_manager->getPoint(point_key)->getPointsAtTime(current_time);

        for (const auto& single_point : pointData) {

            auto ellipse = addEllipse(single_point.y * xAspect, single_point.x * yAspect, 10.0, 10.0, QPen(plot_color));
            _points.append(ellipse);
        }
        i ++;
    }
}
