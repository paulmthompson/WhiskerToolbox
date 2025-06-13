#include "Media_Window.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Lines/lines.hpp"
#include "DataManager/Lines/utils/line_geometry.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Masks/masks.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"
#include "Media_Widget/MediaText_Widget/MediaText_Widget/MediaText_Widget.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include "TimeFrame.hpp"


#include <QElapsedTimer>
#include <QFont>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
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
        _addRemoveData();
    });
}

void Media_Window::addLineDataToScene(std::string const & line_key) {
    auto line_config = std::make_unique<LineDisplayOptions>();

    // Assign color based on the current number of line configs
    line_config->hex_color = DefaultDisplayValues::getColorForIndex(_line_configs.size());

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

void Media_Window::addMaskDataToScene(std::string const & mask_key) {
    auto mask_config = std::make_unique<MaskDisplayOptions>();

    // Assign color based on the current number of mask configs
    mask_config->hex_color = DefaultDisplayValues::getColorForIndex(_mask_configs.size());

    _mask_configs[mask_key] = std::move(mask_config);
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

void Media_Window::_clearMaskBoundingBoxes() {
    for (auto boundingBoxItem: _mask_bounding_boxes) {
        removeItem(boundingBoxItem);
    }

    for (auto boundingBoxItem: _mask_bounding_boxes) {
        delete boundingBoxItem;
    }
    _mask_bounding_boxes.clear();
}

void Media_Window::_clearMaskOutlines() {
    for (auto outlineItem: _mask_outlines) {
        removeItem(outlineItem);
    }

    for (auto outlineItem: _mask_outlines) {
        delete outlineItem;
    }
    _mask_outlines.clear();
}

void Media_Window::removeMaskDataFromScene(std::string const & mask_key) {
    auto maskItem = _mask_configs.find(mask_key);
    if (maskItem != _mask_configs.end()) {
        _mask_configs.erase(maskItem);
    }

    UpdateCanvas();
}

void Media_Window::addPointDataToScene(std::string const & point_key) {
    auto point_config = std::make_unique<PointDisplayOptions>();

    // Assign color based on the current number of point configs
    point_config->hex_color = DefaultDisplayValues::getColorForIndex(_point_configs.size());

    _point_configs[point_key] = std::move(point_config);
    UpdateCanvas();
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

void Media_Window::addDigitalIntervalSeries(std::string const & key) {
    auto interval_config = std::make_unique<DigitalIntervalDisplayOptions>();

    // Assign color based on the current number of interval configs
    interval_config->hex_color = DefaultDisplayValues::getColorForIndex(_interval_configs.size());

    _interval_configs[key] = std::move(interval_config);
    UpdateCanvas();
}

void Media_Window::removeDigitalIntervalSeries(std::string const & key) {
    auto item = _interval_configs.find(key);
    if (item != _interval_configs.end()) {
        _interval_configs.erase(item);
    }

    UpdateCanvas();
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
    auto tensor_config = std::make_unique<TensorDisplayOptions>();

    // Assign color based on the current number of tensor configs
    tensor_config->hex_color = DefaultDisplayValues::getColorForIndex(_tensor_configs.size());

    _tensor_configs[tensor_key] = std::move(tensor_config);

    UpdateCanvas();
}

void Media_Window::removeTensorDataFromScene(std::string const & tensor_key) {
    auto tensorItem = _tensor_configs.find(tensor_key);
    if (tensorItem != _tensor_configs.end()) {
        _tensor_configs.erase(tensorItem);
    }

    UpdateCanvas();
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

void Media_Window::setTextWidget(MediaText_Widget * text_widget) {
    _text_widget = text_widget;
}

void Media_Window::_plotTextOverlays() {
    if (!_text_widget) {
        return;
    }

    // Get enabled text overlays from the widget
    auto text_overlays = _text_widget->getEnabledTextOverlays();

    for (auto const & overlay: text_overlays) {
        if (!overlay.enabled) {
            continue;
        }

        // Calculate position based on relative coordinates (0.0-1.0)
        float x_pos = overlay.x_position * static_cast<float>(_canvasWidth);
        float y_pos = overlay.y_position * static_cast<float>(_canvasHeight);

        // Create text item
        auto text_item = addText(overlay.text);

        // Set font and size
        QFont font = text_item->font();
        font.setPointSize(overlay.font_size);
        text_item->setFont(font);

        // Set color
        QColor text_color(overlay.color);
        text_item->setDefaultTextColor(text_color);

        // Handle orientation
        if (overlay.orientation == TextOrientation::Vertical) {
            text_item->setRotation(90.0);// Rotate 90 degrees for vertical text
        }

        // Set position
        text_item->setPos(x_pos, y_pos);

        // Add to our collection for cleanup
        _text_items.append(text_item);
    }
}

void Media_Window::_clearTextOverlays() {
    for (auto text_item: _text_items) {
        removeItem(text_item);
    }
    for (auto text_item: _text_items) {
        delete text_item;
    }
    _text_items.clear();
}

void Media_Window::LoadFrame(int frame_id) {
    // Get MediaData
    auto media = _data_manager->getData<MediaData>("media");
    media->LoadFrame(frame_id);

    UpdateCanvas();
}

void Media_Window::UpdateCanvas() {

    std::cout << "Update Canvas called" << std::endl;

    _clearLines();
    _clearPoints();
    _clearMasks();
    _clearMaskBoundingBoxes();
    _clearMaskOutlines();
    _clearIntervals();
    _clearTensors();
    _clearTextOverlays();

    //_convertNewMediaToQImage();
    auto _media = _data_manager->getData<MediaData>("media");
    auto const current_time = _data_manager->getCurrentTime();
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

    _plotDigitalIntervalBorders();

    _plotTensorData();

    _plotTextOverlays();

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
    auto const current_time = _data_manager->getCurrentTime();
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
        if (_drawing_mode) {
            auto pos = event->scenePos();
            _drawing_points.clear();
            _drawing_points.push_back(pos);
            _is_drawing = true;
        }
        emit rightClick(event->scenePos().x(), event->scenePos().y());
        emit rightClickMedia(
                event->scenePos().x() / getXAspect(),
                event->scenePos().y() / getYAspect());

    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}
void Media_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
    if (event->button() == Qt::LeftButton && _is_drawing) {
        _is_drawing = false;
        emit leftRelease();
    } else if (event->button() == Qt::RightButton && _is_drawing) {
        _is_drawing = false;
        emit rightRelease();
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
    auto const current_time = _data_manager->getCurrentTime();
    auto xAspect = getXAspect();
    auto yAspect = getYAspect();

    for (auto const & [line_key, _line_config]: _line_configs) {

        if (!_line_config.get()->is_visible) continue;

        auto plot_color = plot_color_with_alpha(_line_config.get());

        auto line_data = _data_manager->getData<LineData>(line_key);
        auto lineData = line_data->getLinesAtTime(current_time);

        // Check for line-specific image size scaling
        auto image_size = line_data->getImageSize();

        if (image_size.height != -1) {
            auto const line_height = static_cast<float>(image_size.height);
            yAspect = static_cast<float>(_canvasHeight) / line_height;
        }

        if (image_size.width != -1) {
            auto const line_width = static_cast<float>(image_size.width);
            xAspect = static_cast<float>(_canvasWidth) / line_width;
        }

        if (lineData.empty()) {
            continue;
        }

        for (int line_idx = 0; line_idx < static_cast<int>(lineData.size()); ++line_idx) {
            auto const & single_line = lineData[line_idx];

            if (single_line.empty()) {
                continue;
            }

            // Check if this line is selected
            bool is_selected = (_line_config.get()->selected_line_index == line_idx);

            // Use segment if enabled, otherwise use full line
            Line2D line_to_plot;
            if (_line_config.get()->show_segment) {
                float start_percentage = static_cast<float>(_line_config.get()->segment_start_percentage) / 100.0f;
                float end_percentage = static_cast<float>(_line_config.get()->segment_end_percentage) / 100.0f;
                line_to_plot = get_segment_between_percentages(single_line, start_percentage, end_percentage);

                // If segment is empty (invalid percentages), skip this line
                if (line_to_plot.empty()) {
                    continue;
                }
            } else {
                line_to_plot = single_line;
            }

            QPainterPath path = QPainterPath();

            auto single_line_thres = 1000.0;

            path.moveTo(QPointF(static_cast<float>(line_to_plot[0].x) * xAspect, static_cast<float>(line_to_plot[0].y) * yAspect));

            for (size_t i = 1; i < line_to_plot.size(); i++) {
                auto dx = line_to_plot[i].x - line_to_plot[i - 1].x;
                auto dy = line_to_plot[i].y - line_to_plot[i - 1].y;
                auto d = std::sqrt((dx * dx) + (dy * dy));
                if (d > single_line_thres) {
                    path.moveTo(QPointF(static_cast<float>(line_to_plot[i].x) * xAspect, static_cast<float>(line_to_plot[i].y) * yAspect));
                } else {
                    path.lineTo(QPointF(static_cast<float>(line_to_plot[i].x) * xAspect, static_cast<float>(line_to_plot[i].y) * yAspect));
                }
            }

            // Create pen with configurable thickness - selected lines are thicker and have different color
            QPen linePen;
            if (is_selected) {
                linePen.setColor(QColor(255, 0, 0));                     // Red for selected lines
                linePen.setWidth(_line_config.get()->line_thickness + 2);// Thicker for selected
                linePen.setStyle(Qt::DashLine);                          // Dashed line for selected
            } else {
                linePen.setColor(plot_color);
                linePen.setWidth(_line_config.get()->line_thickness);
            }

            auto linePath = addPath(path, linePen);
            _line_paths.append(linePath);

            // Add dot at line base (always filled) - selected lines have red dot
            QColor dot_color = is_selected ? QColor(255, 0, 0) : plot_color;
            auto ellipse = addEllipse(
                    static_cast<float>(line_to_plot[0].x) * xAspect - 2.5,
                    static_cast<float>(line_to_plot[0].y) * yAspect - 2.5,
                    5.0, 5.0,
                    QPen(dot_color),
                    QBrush(dot_color));
            _points.append(ellipse);

            // If show_points is enabled, add open circles at each point on the line
            if (_line_config.get()->show_points) {
                // Create pen and brush for open circles
                QPen pointPen(dot_color);
                pointPen.setWidth(1);

                // Empty brush for open circles
                QBrush emptyBrush(Qt::NoBrush);

                // Start from the second point (first one is already shown as filled)
                for (size_t i = 1; i < line_to_plot.size(); i++) {
                    auto ellipse = addEllipse(
                            static_cast<float>(line_to_plot[i].x) * xAspect - 2.5,
                            static_cast<float>(line_to_plot[i].y) * yAspect - 2.5,
                            5.0, 5.0,
                            pointPen,
                            emptyBrush);
                    _points.append(ellipse);
                }
            }

            // If position marker is enabled, add a marker at the specified percentage
            if (_line_config.get()->show_position_marker) {
                float percentage = static_cast<float>(_line_config.get()->position_percentage) / 100.0f;
                Point2D<float> marker_pos = get_position_at_percentage(line_to_plot, percentage);

                float const marker_x = marker_pos.x * xAspect;
                float const marker_y = marker_pos.y * yAspect;

                // Create a distinctive marker (filled circle with border)
                QPen markerPen(QColor(255, 255, 255));// White border
                markerPen.setWidth(2);
                QBrush markerBrush(dot_color);// Same color as line but filled

                auto marker = addEllipse(
                        marker_x - 4.0f,
                        marker_y - 4.0f,
                        8.0f, 8.0f,
                        markerPen,
                        markerBrush);
                _points.append(marker);
            }
        }
    }
}

void Media_Window::_plotMaskData() {
    auto const current_time = _data_manager->getCurrentTime();

    for (auto const & [mask_key, _mask_config]: _mask_configs) {
        if (!_mask_config.get()->is_visible) continue;

        auto plot_color = plot_color_with_alpha(_mask_config.get());

        auto mask = _data_manager->getData<MaskData>(mask_key);
        auto image_size = mask->getImageSize();

        // Check for preview data first
        std::vector<Mask2D> maskData;
        std::vector<Mask2D> maskData2;

        if (_mask_preview_active && _preview_mask_data.count(mask_key) > 0) {
            // Use preview data
            maskData = _preview_mask_data[mask_key];
            maskData2.clear();// No time -1 data for preview
        } else {
            // Use original data
            maskData = mask->getAtTime(current_time);
            maskData2 = mask->getAtTime(-1);
        }

        _plotSingleMaskData(maskData, image_size, plot_color);
        _plotSingleMaskData(maskData2, image_size, plot_color);

        // Plot bounding boxes if enabled
        if (_mask_config.get()->show_bounding_box) {
            // Calculate aspect ratios for scaling coordinates
            auto xAspect = getXAspect();
            auto yAspect = getYAspect();

            // For current time masks
            for (auto const & single_mask: maskData) {
                if (!single_mask.empty()) {
                    auto bounding_box = get_bounding_box(single_mask);
                    Point2D<float> min_point = bounding_box.first;
                    Point2D<float> max_point = bounding_box.second;

                    // Scale coordinates to canvas
                    float min_x = min_point.x * xAspect;
                    float min_y = min_point.y * yAspect;
                    float max_x = max_point.x * xAspect;
                    float max_y = max_point.y * yAspect;

                    // Draw bounding box rectangle (no fill, just outline)
                    QPen boundingBoxPen(plot_color);
                    boundingBoxPen.setWidth(2);
                    QBrush emptyBrush(Qt::NoBrush);

                    auto boundingBoxRect = addRect(min_x, min_y, max_x - min_x, max_y - min_y,
                                                   boundingBoxPen, emptyBrush);
                    _mask_bounding_boxes.append(boundingBoxRect);
                }
            }

            // For time -1 masks
            for (auto const & single_mask: maskData2) {
                if (!single_mask.empty()) {
                    auto bounding_box = get_bounding_box(single_mask);
                    Point2D<float> min_point = bounding_box.first;
                    Point2D<float> max_point = bounding_box.second;

                    // Scale coordinates to canvas
                    float min_x = min_point.x * xAspect;
                    float min_y = min_point.y * yAspect;
                    float max_x = max_point.x * xAspect;
                    float max_y = max_point.y * yAspect;

                    // Draw bounding box rectangle (no fill, just outline)
                    QPen boundingBoxPen(plot_color);
                    boundingBoxPen.setWidth(2);
                    QBrush emptyBrush(Qt::NoBrush);

                    auto boundingBoxRect = addRect(min_x, min_y, max_x - min_x, max_y - min_y,
                                                   boundingBoxPen, emptyBrush);
                    _mask_bounding_boxes.append(boundingBoxRect);
                }
            }
        }

        // Plot outlines if enabled
        if (_mask_config.get()->show_outline) {
            // Calculate aspect ratios for scaling coordinates
            auto xAspect = getXAspect();
            auto yAspect = getYAspect();

            // For current time masks
            for (auto const & single_mask: maskData) {
                if (!single_mask.empty()) {
                    auto outline_points = get_mask_outline(single_mask);

                    if (outline_points.size() >= 2) {
                        QPainterPath outlinePath;

                        // Move to the first point
                        float first_x = outline_points[0].x * xAspect;
                        float first_y = outline_points[0].y * yAspect;
                        outlinePath.moveTo(first_x, first_y);

                        // Connect to all other points
                        for (size_t i = 1; i < outline_points.size(); ++i) {
                            float x = outline_points[i].x * xAspect;
                            float y = outline_points[i].y * yAspect;
                            outlinePath.lineTo(x, y);
                        }

                        // Close the outline by connecting back to start
                        outlinePath.lineTo(first_x, first_y);

                        // Draw thick outline
                        QPen outlinePen(plot_color);
                        outlinePen.setWidth(4);// Thick line

                        auto outlinePathItem = addPath(outlinePath, outlinePen);
                        _mask_outlines.append(outlinePathItem);
                    }
                }
            }

            // For time -1 masks
            for (auto const & single_mask: maskData2) {
                if (!single_mask.empty()) {
                    auto outline_points = get_mask_outline(single_mask);

                    if (outline_points.size() >= 2) {
                        QPainterPath outlinePath;

                        // Move to the first point
                        float first_x = outline_points[0].x * xAspect;
                        float first_y = outline_points[0].y * yAspect;
                        outlinePath.moveTo(first_x, first_y);

                        // Connect to all other points
                        for (size_t i = 1; i < outline_points.size(); ++i) {
                            float x = outline_points[i].x * xAspect;
                            float y = outline_points[i].y * yAspect;
                            outlinePath.lineTo(x, y);
                        }

                        // Close the outline by connecting back to start
                        outlinePath.lineTo(first_x, first_y);

                        // Draw thick outline
                        QPen outlinePen(plot_color);
                        outlinePen.setWidth(4);// Thick line

                        auto outlinePathItem = addPath(outlinePath, outlinePen);
                        _mask_outlines.append(outlinePathItem);
                    }
                }
            }
        }
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

    auto const current_time = _data_manager->getCurrentTime();

    for (auto const & [point_key, _point_config]: _point_configs) {
        if (!_point_config.get()->is_visible) continue;

        auto plot_color = plot_color_with_alpha(_point_config.get());

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

        // Get configurable point size
        float const point_size = static_cast<float>(_point_config.get()->point_size);

        for (auto const & single_point: pointData) {
            float const x_pos = single_point.x * xAspect;
            float const y_pos = single_point.y * yAspect;

            // Create the appropriate marker shape based on configuration
            switch (_point_config.get()->marker_shape) {
                case PointMarkerShape::Circle: {
                    QPen pen(plot_color);
                    pen.setWidth(2);
                    QBrush brush(plot_color);
                    auto ellipse = addEllipse(x_pos - point_size / 2, y_pos - point_size / 2,
                                              point_size, point_size, pen, brush);
                    _points.append(ellipse);
                    break;
                }
                case PointMarkerShape::Square: {
                    QPen pen(plot_color);
                    pen.setWidth(2);
                    QBrush brush(plot_color);
                    auto rect = addRect(x_pos - point_size / 2, y_pos - point_size / 2,
                                        point_size, point_size, pen, brush);
                    _points.append(rect);
                    break;
                }
                case PointMarkerShape::Triangle: {
                    QPen pen(plot_color);
                    pen.setWidth(2);
                    QBrush brush(plot_color);

                    // Create triangle polygon
                    QPolygonF triangle;
                    float const half_size = point_size / 2;
                    triangle << QPointF(x_pos, y_pos - half_size)             // Top point
                             << QPointF(x_pos - half_size, y_pos + half_size) // Bottom left
                             << QPointF(x_pos + half_size, y_pos + half_size);// Bottom right

                    auto polygon = addPolygon(triangle, pen, brush);
                    _points.append(polygon);
                    break;
                }
                case PointMarkerShape::Cross: {
                    QPen pen(plot_color);
                    pen.setWidth(3);

                    float const half_size = point_size / 2;
                    // Draw horizontal line
                    auto hLine = addLine(x_pos - half_size, y_pos, x_pos + half_size, y_pos, pen);
                    _points.append(hLine);

                    // Draw vertical line
                    auto vLine = addLine(x_pos, y_pos - half_size, x_pos, y_pos + half_size, pen);
                    _points.append(vLine);
                    break;
                }
                case PointMarkerShape::X: {
                    QPen pen(plot_color);
                    pen.setWidth(3);

                    float const half_size = point_size / 2;
                    // Draw diagonal line (\)
                    auto dLine1 = addLine(x_pos - half_size, y_pos - half_size,
                                          x_pos + half_size, y_pos + half_size, pen);
                    _points.append(dLine1);

                    // Draw diagonal line (/)
                    auto dLine2 = addLine(x_pos - half_size, y_pos + half_size,
                                          x_pos + half_size, y_pos - half_size, pen);
                    _points.append(dLine2);
                    break;
                }
                case PointMarkerShape::Diamond: {
                    QPen pen(plot_color);
                    pen.setWidth(2);
                    QBrush brush(plot_color);

                    // Create diamond polygon (rotated square)
                    QPolygonF diamond;
                    float const half_size = point_size / 2;
                    diamond << QPointF(x_pos, y_pos - half_size) // Top
                            << QPointF(x_pos + half_size, y_pos) // Right
                            << QPointF(x_pos, y_pos + half_size) // Bottom
                            << QPointF(x_pos - half_size, y_pos);// Left

                    auto polygon = addPolygon(diamond, pen, brush);
                    _points.append(polygon);
                    break;
                }
            }
        }
    }
}

void Media_Window::_plotDigitalIntervalSeries() {
    auto const current_time = _data_manager->getCurrentTime();

    for (auto const & [key, _interval_config]: _interval_configs) {
        if (!_interval_config.get()->is_visible) continue;

        // Only render if using Box plotting style
        if (_interval_config.get()->plotting_style != IntervalPlottingStyle::Box) continue;

        auto plot_color = plot_color_with_alpha(_interval_config.get());

        auto interval_series = _data_manager->getData<DigitalIntervalSeries>(key);

        // Get the timeframe for this interval series
        auto interval_timeframe_key = _data_manager->getTimeFrame(key);
        if (interval_timeframe_key.empty()) {
            std::cerr << "Error: No timeframe found for digital interval series: " << key << std::endl;
            continue;
        }

        // Check if we need to do timeframe conversion
        bool needs_conversion = (interval_timeframe_key != "time");
        std::shared_ptr<TimeFrame> video_timeframe = nullptr;
        std::shared_ptr<TimeFrame> interval_timeframe = nullptr;

        if (needs_conversion) {
            video_timeframe = _data_manager->getTime("time");
            interval_timeframe = _data_manager->getTime(interval_timeframe_key);

            if (!video_timeframe) {
                std::cerr << "Error: Could not get video timeframe 'time' for interval conversion" << std::endl;
                continue;
            }
            if (!interval_timeframe) {
                std::cerr << "Error: Could not get interval timeframe '" << interval_timeframe_key
                          << "' for series: " << key << std::endl;
                continue;
            }
        }

        // Generate relative times based on frame range setting
        std::vector<int> relative_times;
        int const frame_range = _interval_config->frame_range;
        for (int i = -frame_range; i <= frame_range; ++i) {
            relative_times.push_back(i);
        }

        int const square_size = _interval_config->box_size;

        // Calculate position based on location setting
        int start_x, start_y;
        switch (_interval_config->location) {
            case IntervalLocation::TopLeft:
                start_x = 0;
                start_y = 0;
                break;
            case IntervalLocation::TopRight:
                start_x = _canvasWidth - square_size * static_cast<int>(relative_times.size());
                start_y = 0;
                break;
            case IntervalLocation::BottomLeft:
                start_x = 0;
                start_y = _canvasHeight - square_size;
                break;
            case IntervalLocation::BottomRight:
                start_x = _canvasWidth - square_size * static_cast<int>(relative_times.size());
                start_y = _canvasHeight - square_size;
                break;
        }

        for (size_t i = 0; i < relative_times.size(); ++i) {
            int const video_time = current_time + relative_times[i];
            int query_time = video_time;// Default: no conversion needed

            if (needs_conversion) {
                // Convert from video timeframe ("time") to interval series timeframe
                // 1. Convert video time index to actual time value
                int const video_time_value = video_timeframe->getTimeAtIndex(TimeIndex(video_time));

                // 2. Convert time value to index in interval series timeframe
                query_time = interval_timeframe->getIndexAtTime(static_cast<float>(video_time_value));
            }

            bool const event_present = interval_series->isEventAtTime(query_time);

            auto color = event_present ? plot_color : QColor(255, 255, 255, 10);// Transparent if no event

            auto intervalPixmap = addRect(
                    start_x + i * square_size,
                    start_y,
                    square_size,
                    square_size,
                    QPen(Qt::black),// Black border
                    QBrush(color)   // Fill with color if event is present
            );

            _intervals.append(intervalPixmap);
        }
    }
}

void Media_Window::_plotDigitalIntervalBorders() {
    auto const current_time = _data_manager->getCurrentTime();

    for (auto const & [key, _interval_config]: _interval_configs) {
        if (!_interval_config.get()->is_visible) continue;

        // Only render if using Border plotting style
        if (_interval_config.get()->plotting_style != IntervalPlottingStyle::Border) continue;

        auto interval_series = _data_manager->getData<DigitalIntervalSeries>(key);

        // Get the timeframe for this interval series
        auto interval_timeframe_key = _data_manager->getTimeFrame(key);
        if (interval_timeframe_key.empty()) {
            std::cerr << "Error: No timeframe found for digital interval series: " << key << std::endl;
            continue;
        }

        // Check if an interval is present at the current frame
        bool interval_present = false;
        if (interval_timeframe_key != "time") {
            // Need timeframe conversion
            auto video_timeframe = _data_manager->getTime("time");
            auto interval_timeframe = _data_manager->getTime(interval_timeframe_key);

            if (!video_timeframe || !interval_timeframe) {
                std::cerr << "Error: Could not get timeframes for interval conversion" << std::endl;
                continue;
            }

            // Convert current video time to interval timeframe
            auto video_time = video_timeframe->getTimeAtIndex(TimeIndex(current_time));
            auto interval_index = interval_timeframe->getIndexAtTime(video_time);
            interval_present = interval_series->isEventAtTime(interval_index);
        } else {
            // Direct comparison (no timeframe conversion needed)
            interval_present = interval_series->isEventAtTime(current_time);
        }

        // If an interval is present, draw a border around the entire image
        if (interval_present) {
            auto plot_color = plot_color_with_alpha(_interval_config.get());

            // Get border thickness from config
            int thickness = _interval_config->border_thickness;

            QPen border_pen(plot_color);
            border_pen.setWidth(thickness);

            // Draw border as 4 rectangles around the edges of the canvas
            // Top border
            auto top_border = addRect(0, 0, _canvasWidth, thickness, border_pen, QBrush(plot_color));
            _intervals.append(top_border);

            // Bottom border
            auto bottom_border = addRect(0, _canvasHeight - thickness, _canvasWidth, thickness, border_pen, QBrush(plot_color));
            _intervals.append(bottom_border);

            // Left border
            auto left_border = addRect(0, 0, thickness, _canvasHeight, border_pen, QBrush(plot_color));
            _intervals.append(left_border);

            // Right border
            auto right_border = addRect(_canvasWidth - thickness, 0, thickness, _canvasHeight, border_pen, QBrush(plot_color));
            _intervals.append(right_border);
        }
    }
}

void Media_Window::_plotTensorData() {

    auto const current_time = _data_manager->getCurrentTime();

    for (auto const & [key, config]: _tensor_configs) {
        if (!config.get()->is_visible) continue;

        auto tensor_data = _data_manager->getData<TensorData>(key);

        auto tensor_shape = tensor_data->getFeatureShape();

        auto tensor_slice = tensor_data->getChannelSlice(current_time, config->display_channel);

        // Create a QImage from the tensor data
        QImage tensor_image(static_cast<int>(tensor_shape[1]), static_cast<int>(tensor_shape[0]), QImage::Format::Format_ARGB32);
        for (size_t y = 0; y < tensor_shape[0]; ++y) {
            for (size_t x = 0; x < tensor_shape[1]; ++x) {
                float const value = tensor_slice[y * tensor_shape[1] + x];
                //int const pixel_value = static_cast<int>(value * 255);// Assuming the tensor values are normalized between 0 and 1

                // Use the config color with alpha
                QColor color(QString::fromStdString(config->hex_color));
                int alpha = std::lround(config->alpha * 255.0f * (value > 0 ? 1.0f : 0.0f));
                QRgb rgb = qRgba(color.red(), color.green(), color.blue(), alpha);

                tensor_image.setPixel(x, y, rgb);
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
    painter.setBrush(QBrush(Qt::white));// Fill the circles with white

    for (auto const & point: _drawing_points) {
        // Draw a filled circle with the current brush size (hover circle radius)
        float const radius = static_cast<float>(_hover_circle_radius);
        painter.drawEllipse(point, radius, radius);
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

void Media_Window::_plotHoverCircle() {
    QPen circlePen(Qt::red);
    circlePen.setWidth(2);

    auto ellipse = addEllipse(_hover_position.x(), _hover_position.y(), _hover_circle_radius * 2, _hover_circle_radius * 2, circlePen);
    _points.append(ellipse);
}

void Media_Window::_addRemoveData() {
    //New data key was added. This is where we may want to repopulate a custom table
}


QRgb plot_color_with_alpha(BaseDisplayOptions const * opts) {
    auto color = QColor(QString::fromStdString(opts->hex_color));
    auto output_color = qRgba(color.red(), color.green(), color.blue(), std::lround(opts->alpha * 255.0f));

    return output_color;
}

bool Media_Window::hasPreviewMaskData(std::string const & mask_key) const {
    return _mask_preview_active && _preview_mask_data.count(mask_key) > 0;
}

std::vector<Mask2D> Media_Window::getPreviewMaskData(std::string const & mask_key, int time) const {

    static_cast<void>(time);

    if (hasPreviewMaskData(mask_key)) {
        return _preview_mask_data.at(mask_key);
    }
    return {};
}

void Media_Window::setPreviewMaskData(std::string const & mask_key,
                                      std::vector<std::vector<Point2D<float>>> const & preview_data,
                                      bool active) {
    if (active) {
        _preview_mask_data[mask_key] = preview_data;
        _mask_preview_active = true;
    } else {
        _preview_mask_data.erase(mask_key);
        _mask_preview_active = !_preview_mask_data.empty();
    }
}
