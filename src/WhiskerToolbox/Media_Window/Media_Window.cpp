
#include "Media_Window.hpp"

#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include "TimeFrame.hpp"

#include "utils/color.hpp"

#include <QElapsedTimer>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>

#include <iostream>


/*

The Media_Window class

*/


Media_Window::Media_Window(std::shared_ptr<DataManager> data_manager, QObject * parent)
    : QGraphicsScene(parent),
      _data_manager{std::move(data_manager)} {
    _createCanvasForData();

    _data_manager->addObserver([this]() {
        UpdateCanvas();
    });
}

void Media_Window::addLineDataToScene(std::string const & line_key) {

    auto line_config = std::make_unique<LineDisplayOptions>();

    _line_configs[line_key] = std::move(line_config);

    UpdateCanvas();
}

void Media_Window::_clearLines() {
    for (auto pathItem: _line_paths) {
        removeItem(pathItem);
    }
    for (auto pathItem: _line_paths) {
        delete pathItem;
    }
    _line_paths.clear();
}

void Media_Window::removeLineDataFromScene(std::string const & line_key) {
    auto lineItem = _line_configs.find(line_key);
    if (lineItem != _line_configs.end()) {
        _line_configs.erase(lineItem);
    }

    UpdateCanvas();
}

void Media_Window::addMaskDataToScene(std::string const & mask_key, std::string const & hex_color, float alpha) {
    if (!isValidHexColor(hex_color)) {
        std::cerr << "Invalid hex color: " << hex_color << std::endl;
        return;
    }
    if (!isValidAlpha(alpha)) {
        std::cerr << "Invalid alpha value: " << alpha << std::endl;
        return;
    }

    _mask_configs[mask_key] = element_config{hex_color, alpha};

    UpdateCanvas();
}

void Media_Window::changeMaskColor(std::string const & mask_key, std::string const & hex_color) {
    if (!isValidHexColor(hex_color)) {
        std::cerr << "Invalid hex color: " << hex_color << std::endl;
        return;
    }
    if (_mask_configs.find(mask_key) == _mask_configs.end()) {
        std::cerr << "Mask key not found: " << mask_key << std::endl;
        return;
    }

    _mask_configs[mask_key].hex_color = hex_color;
}

void Media_Window::changeMaskAlpha(std::string const & line_key, float const alpha) {
    if (!isValidAlpha(alpha)) {
        std::cerr << "Invalid alpha value: " << alpha << std::endl;
        return;
    }
    if (_mask_configs.find(line_key) == _mask_configs.end()) {
        std::cerr << "Mask key not found: " << line_key << std::endl;
        return;
    }

    _mask_configs[line_key].alpha = alpha;
    UpdateCanvas();
}

void Media_Window::changeMaskAlpha(float const alpha) {
    for (auto & [mask_key, mask_config]: _mask_configs) {
        mask_config.alpha = alpha;
    }
    UpdateCanvas();
}

void Media_Window::_clearMasks() {
    for (auto maskItem: _masks) {
        removeItem(maskItem);
    }

    for (auto maskItem: _masks) {
        delete maskItem;
    }
    _masks.clear();
}

void Media_Window::removeMaskDataFromScene(std::string const & mask_key) {
    auto maskItem = _mask_configs.find(mask_key);
    if (maskItem != _mask_configs.end()) {
        _mask_configs.erase(maskItem);
    }

    UpdateCanvas();
}

void Media_Window::addPointDataToScene(std::string const & point_key, std::string const & hex_color, float alpha) {

    if (!isValidHexColor(hex_color)) {
        std::cerr << "Invalid hex color: " << hex_color << std::endl;
        return;
    }
    if (!isValidAlpha(alpha)) {
        std::cerr << "Invalid alpha value: " << alpha << std::endl;
        return;
    }

    _point_configs[point_key] = element_config{hex_color, alpha};

    UpdateCanvas();
}

void Media_Window::changePointColor(std::string const & point_key, std::string const & hex_color) {
    if (!isValidHexColor(hex_color)) {
        std::cerr << "Invalid hex color: " << hex_color << std::endl;
        return;
    }

    if (_point_configs.find(point_key) == _point_configs.end()) {
        std::cerr << "Point key not found: " << point_key << std::endl;
        return;
    }

    _point_configs[point_key].hex_color = hex_color;
}

void Media_Window::_clearPoints() {
    for (auto pathItem: _points) {
        removeItem(pathItem);
    }
    for (auto pathItem: _points) {
        delete pathItem;
    }
    _points.clear();
}

void Media_Window::removePointDataFromScene(std::string const & point_key) {
    auto pointItem = _point_configs.find(point_key);
    if (pointItem != _point_configs.end()) {
        _point_configs.erase(pointItem);
    }

    UpdateCanvas();
}

void Media_Window::setPointAlpha(std::string const & point_key, float const alpha) {
    if (!isValidAlpha(alpha)) {
        std::cerr << "Invalid alpha value: " << alpha << std::endl;
        return;
    }
    if (_point_configs.find(point_key) == _point_configs.end()) {
        std::cerr << "Point key not found: " << point_key << std::endl;
        return;
    }

    _point_configs[point_key].alpha = alpha;
}

void Media_Window::addDigitalIntervalSeries(
        std::string const & key,
        std::string const & hex_color,
        float alpha) {
    if (!isValidHexColor(hex_color)) {
        std::cerr << "Invalid hex color: " << hex_color << std::endl;
        return;
    }
    if (!isValidAlpha(alpha)) {
        std::cerr << "Invalid alpha value: " << alpha << std::endl;
        return;
    }

    _interval_configs[key] = element_config{hex_color, alpha};

    UpdateCanvas();
}

void Media_Window::removeDigitalIntervalSeries(std::string const & key) {
    auto item = _interval_configs.find(key);
    if (item != _interval_configs.end()) {
        _interval_configs.erase(item);
    }
}

void Media_Window::_clearIntervals() {
    for (auto item: _intervals) {
        removeItem(item);
    }

    for (auto item: _intervals) {
        delete item;
    }
    _intervals.clear();
}

void Media_Window::addTensorDataToScene(std::string const & tensor_key) {
    if (_tensor_configs.find(tensor_key) != _tensor_configs.end()) {
        std::cerr << "Tensor key already exists: " << tensor_key << std::endl;
        return;
    }
    _tensor_configs[tensor_key] = tensor_config{0};
}

void Media_Window::removeTensorDataFromScene(std::string const & tensor_key) {
    auto item = _tensor_configs.find(tensor_key);
    if (item != _tensor_configs.end()) {
        _tensor_configs.erase(item);
    }
}

void Media_Window::setTensorChannel(std::string const & tensor_key, int channel) {
    if (_tensor_configs.find(tensor_key) == _tensor_configs.end()) {
        std::cerr << "Tensor key not found: " << tensor_key << std::endl;
        return;
    }

    _tensor_configs[tensor_key].channel = channel;
}

void Media_Window::_clearTensors() {
    for (auto item: _tensors) {
        removeItem(item);
    }

    for (auto item: _tensors) {
        delete item;
    }
    _tensors.clear();
}

void Media_Window::LoadFrame(int frame_id) {
    // Get MediaData
    auto media = _data_manager->getData<MediaData>("media");
    media->LoadFrame(frame_id);

    UpdateCanvas();
}

void Media_Window::UpdateCanvas() {
    _clearLines();
    _clearPoints();
    _clearMasks();
    _clearIntervals();
    _clearTensors();

    //_convertNewMediaToQImage();
    auto _media = _data_manager->getData<MediaData>("media");
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto media_data = _media->getProcessedData(current_time);

    auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat());

    auto new_image = unscaled_image.scaled(
            _canvasWidth,
            _canvasHeight,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);

    _canvasPixmap->setPixmap(QPixmap::fromImage(new_image));
    _canvasImage = new_image;

    // Check for manual selection with the currently rendered frame;

    _plotLineData();

    _plotMaskData();

    _plotPointData();

    _plotDigitalIntervalSeries();

    _plotTensorData();

    if (_show_hover_circle) {
        _plotHoverCircle();
    }

    // Save the entire QGraphicsScene as an image
    QImage scene_image(_canvasWidth, _canvasHeight, QImage::Format_ARGB32);
    scene_image.fill(Qt::transparent);// Optional: fill with transparent background
    QPainter painter(&scene_image);
    this->render(&painter);

    emit canvasUpdated(scene_image);
}


//Load media designated by frame_id
//Media frame is loaded. It is then scaled to the
//Canvas size, and the canvas is updated
void Media_Window::_convertNewMediaToQImage() {
    auto _media = _data_manager->getData<MediaData>("media");
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto media_data = _media->getProcessedData(current_time);

    auto unscaled_image = QImage(&media_data[0],
                                 _media->getWidth(),
                                 _media->getHeight(),
                                 _getQImageFormat());

    _canvasImage = unscaled_image.scaled(_canvasWidth, _canvasHeight);
}

QImage::Format Media_Window::_getQImageFormat() {

    auto _media = _data_manager->getData<MediaData>("media");
    switch (_media->getFormat()) {
        case MediaData::DisplayFormat::Gray:
            return QImage::Format_Grayscale8;
        case MediaData::DisplayFormat::Color:
            return QImage::Format_RGBA8888;
    }
}

void Media_Window::_createCanvasForData() {

    auto image_format = _getQImageFormat();

    _mediaImage = QImage(_canvasWidth, _canvasHeight, image_format);
    _canvasImage = QImage(_canvasWidth, _canvasHeight, image_format);

    _canvasPixmap = addPixmap(QPixmap::fromImage(_canvasImage));
}

void Media_Window::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        if (_drawing_mode) {
            auto pos = event->scenePos();
            _drawing_points.clear();
            _drawing_points.push_back(pos);
            _is_drawing = true;
        }
        emit leftClick(event->scenePos().x(), event->scenePos().y());
        emit leftClickMedia(
                event->scenePos().x() / getXAspect(),
                event->scenePos().y() / getYAspect());
    } else if (event->button() == Qt::RightButton) {

    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}
void Media_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
    if (event->button() == Qt::LeftButton && _is_drawing) {
        _is_drawing = false;
        emit leftRelease();
    }
    QGraphicsScene::mouseReleaseEvent(event);
}
void Media_Window::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {

    auto pos = event->scenePos();

    _hover_position = pos;

    if (_is_drawing) {

        _drawing_points.push_back(pos);
    }

    emit mouseMove(event->scenePos().x(), event->scenePos().y());

    QGraphicsScene::mouseMoveEvent(event);

}

float Media_Window::getXAspect() const {

    auto _media = _data_manager->getData<MediaData>("media");

    float const scale_width = static_cast<float>(_canvasWidth) / static_cast<float>(_media->getWidth());

    return scale_width;
}

float Media_Window::getYAspect() const {

    auto _media = _data_manager->getData<MediaData>("media");

    float const scale_height = static_cast<float>(_canvasHeight) / static_cast<float>(_media->getHeight());

    return scale_height;
}

void Media_Window::_plotLineData() {
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();
    auto xAspect = getXAspect();
    auto yAspect = getYAspect();

    for (auto const & [line_key, _line_config]: _line_configs) {
        auto plot_color = plot_color_with_alpha(_line_config.get());

        auto lineData = _data_manager->getData<LineData>(line_key)->getLinesAtTime(current_time);

        if (lineData.empty()) {
            continue;
        }

        for (auto const & single_line: lineData) {

            if (single_line.empty()) {
                continue;
            }

            QPainterPath path = QPainterPath();

            auto single_line_thres = 1000.0;

            path.moveTo(QPointF(static_cast<float>(single_line[0].x) * xAspect, static_cast<float>(single_line[0].y) * yAspect));

            for (int i = 1; i < single_line.size(); i++) {
                auto dx = single_line[i].x - single_line[i - 1].x;
                auto dy = single_line[i].y - single_line[i - 1].y;
                auto d = std::sqrt((dx * dx) + (dy * dy));
                if (d > single_line_thres) {
                    path.moveTo(QPointF(static_cast<float>(single_line[i].x) * xAspect, static_cast<float>(single_line[i].y) * yAspect));
                } else {
                    path.lineTo(QPointF(static_cast<float>(single_line[i].x) * xAspect, static_cast<float>(single_line[i].y) * yAspect));
                }
            }

            auto linePath = addPath(path, QPen(plot_color));
            _line_paths.append(linePath);


            // Add dot at line base

            auto ellipse = addEllipse(
                    static_cast<float>(single_line[0].x) * xAspect - 2.5,
                    static_cast<float>(single_line[0].y) * yAspect - 2.5,
                    5.0, 5.0,
                    QPen(plot_color),
                    QBrush(plot_color));
            _points.append(ellipse);

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

void Media_Window::_plotMaskData() {
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (auto const & [mask_key, _mask_config]: _mask_configs) {
        auto plot_color = plot_color_with_alpha(_mask_config);

        auto mask = _data_manager->getData<MaskData>(mask_key);
        auto image_size = mask->getImageSize();

        auto const & maskData = mask->getMasksAtTime(current_time);

        _plotSingleMaskData(maskData, image_size, plot_color);

        auto const & maskData2 = mask->getMasksAtTime(-1);

        _plotSingleMaskData(maskData2, image_size, plot_color);
    }
}

void Media_Window::_plotSingleMaskData(std::vector<Mask2D> const & maskData, ImageSize mask_size, QRgb plot_color) {
    for (auto const & single_mask: maskData) {

        QImage unscaled_mask_image(mask_size.width, mask_size.height, QImage::Format::Format_ARGB32);

        unscaled_mask_image.fill(0);

        for (auto const point: single_mask) {
            unscaled_mask_image.setPixel(
                    QPoint(std::lround(point.x), std::lround(point.y)),
                    plot_color);
        }

        auto scaled_mask_image = unscaled_mask_image.scaled(_canvasWidth, _canvasHeight);

        auto maskPixmap = addPixmap(QPixmap::fromImage(scaled_mask_image));

        _masks.append(maskPixmap);
    }
}

void Media_Window::_plotPointData() {
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    int i = 0;

    for (auto const & [point_key, _point_config]: _point_configs) {

        auto plot_color = plot_color_with_alpha(_point_config);

        auto point = _data_manager->getData<PointData>(point_key);

        auto xAspect = getXAspect();
        auto yAspect = getYAspect();

        auto image_size = point->getImageSize();

        if (image_size.height != -1) {
            auto const mask_height = static_cast<float>(image_size.height);
            yAspect = static_cast<float>(_canvasHeight) / mask_height;
        }

        if (image_size.width != -1) {
            auto const mask_width = static_cast<float>(image_size.width);
            xAspect = static_cast<float>(_canvasWidth) / mask_width;
        }

        auto pointData = point->getPointsAtTime(current_time);


        auto pen = QPen(plot_color);
        pen.setWidth(3);
        for (auto const & single_point: pointData) {
            auto ellipse = addEllipse(single_point.y * xAspect, single_point.x * yAspect, 10.0, 10.0, pen);
            _points.append(ellipse);
        }
        i++;
    }
}

void Media_Window::_plotDigitalIntervalSeries() {
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (auto const & [key, _interval_config]: _interval_configs) {
        auto plot_color = plot_color_with_alpha(_interval_config);

        auto interval_series = _data_manager->getData<DigitalIntervalSeries>(key);

        std::vector<int> relative_times = {-2, -1, 0, 1, 2};
        int const square_size = 20;
        int const top_right_x = _canvasWidth - square_size;
        int const top_right_y = 0;

        for (int i = 0; i < relative_times.size(); ++i) {
            int const time = current_time + relative_times[i];
            bool const event_present = interval_series->isEventAtTime(time);

            auto color = event_present ? plot_color : QColor(255, 255, 255, 10);// Transparent if no event

            auto intervalPixmap = addRect(
                    top_right_x - (relative_times.size() - 1 - i) * square_size,
                    top_right_y,
                    square_size,
                    square_size,
                    QPen(Qt::black),// Black border
                    QBrush(color)   // Fill with color if event is present
            );

            _intervals.append(intervalPixmap);
        }
    }
}

void Media_Window::_plotTensorData() {
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    for (auto const & [key, config]: _tensor_configs) {
        auto tensor_data = _data_manager->getData<TensorData>(key);

        auto tensor_shape = tensor_data->getFeatureShape();

        auto tensor_slice = tensor_data->getChannelSlice(current_time, config.channel);

        // Create a QImage from the tensor data
        QImage tensor_image(static_cast<int>(tensor_shape[1]), static_cast<int>(tensor_shape[0]), QImage::Format::Format_ARGB32);
        for (int y = 0; y < tensor_shape[0]; ++y) {
            for (int x = 0; x < tensor_shape[1]; ++x) {
                float const value = tensor_slice[y * tensor_shape[1] + x];
                int const pixel_value = static_cast<int>(value * 255);// Assuming the tensor values are normalized between 0 and 1
                tensor_image.setPixel(x, y, qRgba(pixel_value, 0, 0, pixel_value));
            }
        }

        // Scale the tensor image to the size of the canvas
        QImage const scaled_tensor_image = tensor_image.scaled(_canvasWidth, _canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        auto tensor_pixmap = addPixmap(QPixmap::fromImage(scaled_tensor_image));

        _tensors.append(tensor_pixmap);
    }
}

std::vector<uint8_t> Media_Window::getDrawingMask() {
    // Create a QImage with _canvasWidth and _canvasHeight
    QImage maskImage(_canvasWidth, _canvasHeight, QImage::Format_Grayscale8);
    maskImage.fill(0);

    QPainter painter(&maskImage);
    painter.setPen(Qt::white);

    for (auto const & point: _drawing_points) {
        painter.drawPoint(point.toPoint());
    }
    painter.end();

    // Scale the QImage to the size of the media
    auto media = _data_manager->getData<MediaData>("media");
    int const mediaWidth = media->getWidth();
    int const mediaHeight = media->getHeight();
    QImage scaledMaskImage = maskImage.scaled(mediaWidth, mediaHeight);

    // Convert the QImage to a std::vector<uint8_t>
    std::vector<uint8_t> mask(scaledMaskImage.bits(), scaledMaskImage.bits() + scaledMaskImage.sizeInBytes());

    return mask;
}

void Media_Window::setShowHoverCircle(bool show) {
    _show_hover_circle = show;
    if (_show_hover_circle) {
        std::cout << "Hover circle enabled" << std::endl;

        connect(this, &Media_Window::mouseMove, this, &Media_Window::UpdateCanvas);
    } else {
        std::cout << "Hover circle disabled" << std::endl;
        disconnect(this, &Media_Window::mouseMove, this, &Media_Window::UpdateCanvas);
    }

}

void Media_Window::setHoverCircleRadius(int radius) {
    _hover_circle_radius = radius;
}

void Media_Window::_plotHoverCircle()
{
    QPen circlePen(Qt::red);
    circlePen.setWidth(2);

    auto ellipse = addEllipse(_hover_position.x(), _hover_position.y(), _hover_circle_radius*2, _hover_circle_radius*2, circlePen);
    _points.append(ellipse);

}


QRgb plot_color_with_alpha(element_config const & elem) {
    auto color = QColor(QString::fromStdString(elem.hex_color));
    auto output_color = qRgba(color.red(), color.green(), color.blue(), std::lround(elem.alpha * 255.0f));

    return output_color;
}


QRgb plot_color_with_alpha(BaseDisplayOptions const * opts)
{
    auto color = QColor(QString::fromStdString(opts->hex_color));
    auto output_color = qRgba(color.red(), color.green(), color.blue(), std::lround(opts->alpha * 255.0f));

    return output_color;
}
