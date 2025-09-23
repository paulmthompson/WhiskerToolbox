#include "Media_Window.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include "Media_Widget/DisplayOptions/DisplayOptions.hpp"
#include "Media_Widget/MediaProcessing_Widget/MediaProcessing_Widget.hpp"
#include "Media_Widget/MediaText_Widget/MediaText_Widget.hpp"
#include "Media_Widget/Media_Widget.hpp"
#include "TimeFrame/TimeFrame.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include <QElapsedTimer>
#include <QFont>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QImage>
#include <QPainter>
#include <algorithm>

#include <iostream>

/*

The Media_Window class

*/


Media_Window::Media_Window(std::shared_ptr<DataManager> data_manager, QObject * parent)
    : QGraphicsScene(parent),
      _data_manager{std::move(data_manager)} {

    _data_manager->addObserver([this]() {
        _addRemoveData();
    });

    _canvasImage = QImage(_canvasWidth, _canvasHeight, QImage::Format_ARGB32);
    _canvasPixmap = addPixmap(QPixmap::fromImage(_canvasImage));
}

Media_Window::~Media_Window() {
    // Clear all items from the scene - this automatically removes and deletes all QGraphicsItems
    clear();

    // Just clear the containers since the items are already deleted by clear()
    _line_paths.clear();
    _masks.clear();
    _mask_bounding_boxes.clear();
    _mask_outlines.clear();
    _points.clear();
    _intervals.clear();
    _tensors.clear();
    _text_items.clear();
}

void Media_Window::addMediaDataToScene(std::string const & media_key) {
    auto media_config = std::make_unique<MediaDisplayOptions>();

    _media_configs[media_key] = std::move(media_config);

    UpdateCanvas();
}

void Media_Window::_clearMedia() {
    // Set to black
    _canvasImage.fill(Qt::black);
    _canvasPixmap->setPixmap(QPixmap::fromImage(_canvasImage));
}

void Media_Window::removeMediaDataFromScene(std::string const & media_key) {
    auto mediaItem = _media_configs.find(media_key);
    if (mediaItem != _media_configs.end()) {
        _media_configs.erase(mediaItem);
    }

    UpdateCanvas();
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
    if (_debug_performance) {
        std::cout << "CLEARING POINTS - Count before: " << _points.size() << std::endl;
    }

    for (auto pathItem: _points) {
        removeItem(pathItem);
    }
    for (auto pathItem: _points) {
        delete pathItem;
    }
    _points.clear();

    if (_debug_performance) {
        std::cout << "  Points cleared. Count after: " << _points.size() << std::endl;
        std::cout << "  Hover circle item still exists: " << (_hover_circle_item ? "YES" : "NO") << std::endl;
    }
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
        float const x_pos = overlay.x_position * static_cast<float>(_canvasWidth);
        float const y_pos = overlay.y_position * static_cast<float>(_canvasHeight);

        // Create text item
        auto text_item = addText(overlay.text);

        // Set font and size
        QFont font = text_item->font();
        font.setPointSize(overlay.font_size);
        text_item->setFont(font);

        // Set color
        QColor const text_color(overlay.color);
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
    // Get MediaData using the active media key
    for (auto const & [media_key, media_config]: _media_configs) {
        if (!media_config.get()->is_visible) {
            continue;
        }

        auto media = _data_manager->getData<MediaData>(media_key);
        if (!media) {
            std::cerr << "Warning: No media data found for key '" << media_key << "'" << std::endl;
            return;
        }
        media->LoadFrame(frame_id);
    }

    // Clear any accumulated drawing points when changing frames
    // This ensures no cross-frame accumulation and explains why lag disappears on frame change
    _drawing_points.clear();
    _is_drawing = false;

    UpdateCanvas();
}

void Media_Window::UpdateCanvas() {

    if (_debug_performance) {
        std::cout << "========== Update Canvas called ==========" << std::endl;

        // Debug: Show current item counts before clearing
        std::cout << "BEFORE CLEAR - Items in scene: " << items().size() << std::endl;
        std::cout << "  Lines: " << _line_paths.size() << std::endl;
        std::cout << "  Points: " << _points.size() << std::endl;
        std::cout << "  Masks: " << _masks.size() << std::endl;
        std::cout << "  Mask bounding boxes: " << _mask_bounding_boxes.size() << std::endl;
        std::cout << "  Mask outlines: " << _mask_outlines.size() << std::endl;
        std::cout << "  Intervals: " << _intervals.size() << std::endl;
        std::cout << "  Tensors: " << _tensors.size() << std::endl;
        std::cout << "  Text items: " << _text_items.size() << std::endl;
        std::cout << "  Drawing points accumulated: " << _drawing_points.size() << std::endl;
        std::cout << "  Hover circle item exists: " << (_hover_circle_item ? "YES" : "NO") << std::endl;
    }

    _clearLines();
    _clearPoints();
    _clearMasks();
    _clearMaskBoundingBoxes();
    _clearMaskOutlines();
    _clearIntervals();
    _clearTensors();
    _clearTextOverlays();
    _clearMedia();

    _plotMediaData();

    _plotLineData();

    _plotMaskData();

    _plotPointData();

    _plotDigitalIntervalSeries();

    _plotDigitalIntervalBorders();

    _plotTensorData();

    _plotTextOverlays();

    // Note: Hover circle is now handled efficiently via _updateHoverCirclePosition()
    // and doesn't need to be redrawn on every UpdateCanvas() call

    if (_debug_performance) {
        // Debug: Show item counts after plotting
        std::cout << "AFTER PLOTTING - Items in scene: " << items().size() << std::endl;
        std::cout << "  Lines plotted: " << _line_paths.size() << std::endl;
        std::cout << "  Points plotted: " << _points.size() << std::endl;
        std::cout << "  Masks plotted: " << _masks.size() << std::endl;
        std::cout << "  Mask bounding boxes plotted: " << _mask_bounding_boxes.size() << std::endl;
        std::cout << "  Mask outlines plotted: " << _mask_outlines.size() << std::endl;
        std::cout << "  Intervals plotted: " << _intervals.size() << std::endl;
        std::cout << "  Tensors plotted: " << _tensors.size() << std::endl;
        std::cout << "  Text items plotted: " << _text_items.size() << std::endl;
    }

    // Save the entire QGraphicsScene as an image
    QImage scene_image(_canvasWidth, _canvasHeight, QImage::Format_ARGB32);
    scene_image.fill(Qt::transparent);// Optional: fill with transparent background
    QPainter painter(&scene_image);

    // Set the scene rect to match the canvas dimensions
    this->setSceneRect(0, 0, _canvasWidth, _canvasHeight);

    // Render the scene with proper viewport mapping
    this->render(&painter, QRectF(0, 0, _canvasWidth, _canvasHeight),
                 QRect(0, 0, _canvasWidth, _canvasHeight));

    emit canvasUpdated(scene_image);
}


QImage::Format Media_Window::_getQImageFormat(std::string const & media_key) {

    auto _media = _data_manager->getData<MediaData>(media_key);
    if (!_media) {
        // Return a default format if no media is available
        return QImage::Format_Grayscale8;
    }

    // Check bit depth for grayscale images
    if (_media->getFormat() == MediaData::DisplayFormat::Gray) {
        if (_media->is32Bit()) {
            return QImage::Format_Grayscale16;// Use 16-bit for higher precision
        } else {
            return QImage::Format_Grayscale8;// Default 8-bit
        }
    } else {
        // Color format
        return QImage::Format_RGBA8888;
    }
}

void Media_Window::_plotMediaData() {

    auto const current_time = _data_manager->getCurrentTime();

    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

    int total_visible_media = 0;
    std::string active_media_key;
    for (auto const & [media_key, _media_config]: _media_configs) {
        if (!_media_config.get()->is_visible) continue;
        total_visible_media++;
        active_media_key = media_key;
    }

    if (total_visible_media == 0) {
        return;
    }

    QImage unscaled_image;

    if (total_visible_media == 1) {
        auto media = _data_manager->getData<MediaData>(active_media_key);
        if (!media) {
            std::cerr << "Warning: No media data found for key '" << active_media_key << "'" << std::endl;
            return;
        }

        if (media->getFormat() == MediaData::DisplayFormat::Gray) {
            // Handle grayscale images with potential colormap application
            bool apply_colormap = _media_configs[active_media_key].get()->colormap_options.active &&
                                  _media_configs[active_media_key].get()->colormap_options.colormap != ColormapType::None;

            if (media->is8Bit()) {
                // 8-bit grayscale processing
                auto unscaled_image_data_8bit = media->getProcessedData8(current_time);

                if (apply_colormap) {
                    auto colormap_data = ImageProcessing::apply_colormap_for_display(
                            unscaled_image_data_8bit,
                            media->getImageSize(),
                            _media_configs[active_media_key].get()->colormap_options);

                    // Apply colormap and get BGRA data (OpenCV returns BGRA format)
                    unscaled_image = QImage(colormap_data.data(),
                                            media->getWidth(),
                                            media->getHeight(),
                                            QImage::Format_ARGB32)
                                             .copy();
                } else {
                    // No colormap, use original 8-bit grayscale data
                    unscaled_image = QImage(unscaled_image_data_8bit.data(),
                                            media->getWidth(),
                                            media->getHeight(),
                                            QImage::Format_Grayscale8)
                                             .copy();
                }
            } else if (media->is32Bit()) {
                // 32-bit float processing
                auto unscaled_image_data_32bit = media->getProcessedData32(current_time);

                if (apply_colormap) {
                    // TODO: Need to implement apply_colormap_for_display for float data
                    // For now, convert to 8-bit and apply colormap
                    std::vector<uint8_t> converted_8bit;
                    converted_8bit.reserve(unscaled_image_data_32bit.size());

                    for (float pixel_value: unscaled_image_data_32bit) {
                        // Clamp to 0-255 range and convert to uint8_t
                        uint8_t byte_value = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, pixel_value)));
                        converted_8bit.push_back(byte_value);
                    }

                    auto colormap_data = ImageProcessing::apply_colormap_for_display(
                            converted_8bit,
                            media->getImageSize(),
                            _media_configs[active_media_key].get()->colormap_options);

                    // Apply colormap and get BGRA data - make a deep copy to avoid use-after-free
                    unscaled_image = QImage(colormap_data.data(),
                                            media->getWidth(),
                                            media->getHeight(),
                                            QImage::Format_ARGB32)
                                             .copy();
                } else {
                    // No colormap, convert 32-bit float to 16-bit for higher precision display
                    std::vector<uint16_t> converted_16bit;
                    converted_16bit.reserve(unscaled_image_data_32bit.size());

                    for (float pixel_value: unscaled_image_data_32bit) {
                        // Scale from 0-255 range to 0-65535 range
                        uint16_t value_16bit = static_cast<uint16_t>(std::max(0.0f, std::min(255.0f, pixel_value)) * 257.0f);
                        converted_16bit.push_back(value_16bit);
                    }

                    // Create QImage and make a deep copy to avoid use-after-free
                    unscaled_image = QImage(reinterpret_cast<uchar const *>(converted_16bit.data()),
                                            media->getWidth(),
                                            media->getHeight(),
                                            media->getWidth() * sizeof(uint16_t),
                                            QImage::Format_Grayscale16)
                                             .copy();
                }
            }
        } else {
            // Color image processing (always 8-bit for now)
            auto unscaled_image_data = media->getProcessedData8(current_time);
            unscaled_image = QImage(unscaled_image_data.data(),
                                    media->getWidth(),
                                    media->getHeight(),
                                    QImage::Format_RGBA8888);
        }
    }


    // Check for multi-channel mode (multiple enabled grayscale media)
    if (total_visible_media > 1) {
        // Multi-channel mode: combine multiple media with colormaps
        unscaled_image = _combineMultipleMedia();
    }

    auto new_image = unscaled_image.scaled(
            _canvasWidth,
            _canvasHeight,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);

    std::cout << "Scaled image" << std::endl;

    // Check if any masks are in transparency mode
    bool has_transparency_mask = false;
    for (auto const & [mask_key, mask_config]: _mask_configs) {
        if (mask_config->is_visible && mask_config->use_as_transparency) {
            has_transparency_mask = true;
            break;
        }
    }

    // If we have transparency masks, modify the new_image
    if (has_transparency_mask) {
        new_image = _applyTransparencyMasks(new_image);
    }

    _canvasPixmap->setPixmap(QPixmap::fromImage(new_image));
    _canvasImage = new_image;
}


QImage Media_Window::_combineMultipleMedia() {

    auto current_time = _data_manager->getCurrentTime();

    // Loop through configs and get the largest image size
    std::vector<ImageSize> media_sizes;
    for (auto const & [media_key, media_config]: _media_configs) {
        if (!media_config->is_visible) continue;

        auto media = _data_manager->getData<MediaData>(media_key);
        if (!media) continue;

        media_sizes.push_back(media->getImageSize());
    }

    if (media_sizes.empty()) return QImage();

    // Find the maximum width and height
    int width = 0;
    int height = 0;
    for (auto const & size: media_sizes) {
        width = std::max(width, size.width);
        height = std::max(height, size.height);
    }

    // Create combined RGBA image
    QImage combined_image(width, height, QImage::Format_RGBA8888);
    combined_image.fill(qRgba(0, 0, 0, 255));// Start with black background

    for (auto const & [media_key, media_config]: _media_configs) {
        if (!media_config->is_visible) continue;

        auto media = _data_manager->getData<MediaData>(media_key);
        if (!media || media->getFormat() != MediaData::DisplayFormat::Gray) {
            continue;// Skip non-grayscale media
        }

        bool apply_colormap = media_config.get()->colormap_options.active &&
                              media_config.get()->colormap_options.colormap != ColormapType::None;

        if (media->is8Bit()) {
            // Handle 8-bit media data
            auto media_data_8bit = media->getProcessedData8(current_time);

            if (apply_colormap) {
                auto colormap_data = ImageProcessing::apply_colormap_for_display(
                        media_data_8bit,
                        media->getImageSize(),
                        media_config.get()->colormap_options);

                // Use colormap data (BGRA format from OpenCV)
                for (int y = 0; y < media->getHeight(); ++y) {
                    for (int x = 0; x < media->getWidth(); ++x) {
                        int const pixel_idx = (y * media->getWidth() + x) * 4;

                        uint8_t const b = colormap_data[pixel_idx];    // Blue channel
                        uint8_t const g = colormap_data[pixel_idx + 1];// Green channel
                        uint8_t const r = colormap_data[pixel_idx + 2];// Red channel
                        uint8_t const a = colormap_data[pixel_idx + 3];// Alpha channel

                        // Get current pixel from combined image
                        QRgb current_pixel = combined_image.pixel(x, y);

                        // Additive blending (common for multi-channel microscopy)
                        uint8_t const new_r = std::min(255, qRed(current_pixel) + r);
                        uint8_t const new_g = std::min(255, qGreen(current_pixel) + g);
                        uint8_t const new_b = std::min(255, qBlue(current_pixel) + b);

                        combined_image.setPixel(x, y, qRgba(new_r, new_g, new_b, 255));
                    }
                }
            } else {
                // Use 8-bit grayscale data directly (no colormap)
                for (int y = 0; y < media->getHeight(); ++y) {
                    for (int x = 0; x < media->getWidth(); ++x) {
                        int const pixel_idx = y * media->getWidth() + x;
                        uint8_t const gray_value = media_data_8bit[pixel_idx];

                        // Get current pixel from combined image
                        QRgb current_pixel = combined_image.pixel(x, y);

                        // Additive blending
                        uint8_t const new_r = std::min(255, qRed(current_pixel) + gray_value);
                        uint8_t const new_g = std::min(255, qGreen(current_pixel) + gray_value);
                        uint8_t const new_b = std::min(255, qBlue(current_pixel) + gray_value);

                        combined_image.setPixel(x, y, qRgba(new_r, new_g, new_b, 255));
                    }
                }
            }
        } else if (media->is32Bit()) {
            // Handle 32-bit float media data
            auto media_data_32bit = media->getProcessedData32(current_time);

            if (apply_colormap) {
                // Convert to 8-bit for colormap application (temporary until float colormap is implemented)
                std::vector<uint8_t> converted_8bit;
                converted_8bit.reserve(media_data_32bit.size());

                for (float pixel_value: media_data_32bit) {
                    uint8_t byte_value = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, pixel_value)));
                    converted_8bit.push_back(byte_value);
                }

                auto colormap_data = ImageProcessing::apply_colormap_for_display(
                        converted_8bit,
                        media->getImageSize(),
                        media_config.get()->colormap_options);

                // Use colormap data (BGRA format from OpenCV)
                for (int y = 0; y < media->getHeight(); ++y) {
                    for (int x = 0; x < media->getWidth(); ++x) {
                        int const pixel_idx = (y * media->getWidth() + x) * 4;

                        uint8_t const b = colormap_data[pixel_idx];    // Blue channel
                        uint8_t const g = colormap_data[pixel_idx + 1];// Green channel
                        uint8_t const r = colormap_data[pixel_idx + 2];// Red channel
                        uint8_t const a = colormap_data[pixel_idx + 3];// Alpha channel

                        // Get current pixel from combined image
                        QRgb current_pixel = combined_image.pixel(x, y);

                        // Additive blending
                        uint8_t const new_r = std::min(255, qRed(current_pixel) + r);
                        uint8_t const new_g = std::min(255, qGreen(current_pixel) + g);
                        uint8_t const new_b = std::min(255, qBlue(current_pixel) + b);

                        combined_image.setPixel(x, y, qRgba(new_r, new_g, new_b, 255));
                    }
                }
            } else {
                // Use 32-bit float data directly (no colormap)
                for (int y = 0; y < media->getHeight(); ++y) {
                    for (int x = 0; x < media->getWidth(); ++x) {
                        int const pixel_idx = y * media->getWidth() + x;
                        float const float_value = media_data_32bit[pixel_idx];
                        uint8_t const gray_value = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, float_value)));

                        // Get current pixel from combined image
                        QRgb current_pixel = combined_image.pixel(x, y);

                        // Additive blending
                        uint8_t const new_r = std::min(255, qRed(current_pixel) + gray_value);
                        uint8_t const new_g = std::min(255, qGreen(current_pixel) + gray_value);
                        uint8_t const new_b = std::min(255, qBlue(current_pixel) + gray_value);

                        combined_image.setPixel(x, y, qRgba(new_r, new_g, new_b, 255));
                    }
                }
            }
        }
    }

    return combined_image;
}

void Media_Window::mousePressEvent(QGraphicsSceneMouseEvent * event) {
    if (_debug_performance) {
        std::cout << "Mouse PRESS - Button: " << (event->button() == Qt::LeftButton ? "LEFT" : "RIGHT")
                  << ", Drawing mode: " << _drawing_mode << ", Current drawing points: " << _drawing_points.size() << std::endl;
    }

    if (event->button() == Qt::LeftButton) {
        if (_drawing_mode) {
            auto pos = event->scenePos();
            _drawing_points.clear();
            _drawing_points.push_back(pos);
            _is_drawing = true;
            if (_debug_performance) {
                std::cout << "  Started drawing - cleared and added first point" << std::endl;
            }
        }

        // Emit legacy signals (qreal values)
        emit leftClick(event->scenePos().x(), event->scenePos().y());
        emit leftClickMedia(
                event->scenePos().x() / getXAspect(),
                event->scenePos().y() / getYAspect());

        // Emit strong-typed coordinate signals
        CanvasCoordinates const canvas_coords(static_cast<float>(event->scenePos().x()),
                                              static_cast<float>(event->scenePos().y()));
        MediaCoordinates const media_coords(static_cast<float>(event->scenePos().x() / getXAspect()),
                                            static_cast<float>(event->scenePos().y() / getYAspect()));
        emit leftClickCanvas(canvas_coords);
        emit leftClickMediaCoords(media_coords);

    } else if (event->button() == Qt::RightButton) {
        if (_drawing_mode) {
            auto pos = event->scenePos();
            _drawing_points.clear();
            _drawing_points.push_back(pos);
            _is_drawing = true;
        }

        // Emit legacy signals (qreal values)
        emit rightClick(event->scenePos().x(), event->scenePos().y());
        emit rightClickMedia(
                event->scenePos().x() / getXAspect(),
                event->scenePos().y() / getYAspect());

        // Emit strong-typed coordinate signals
        CanvasCoordinates const canvas_coords(static_cast<float>(event->scenePos().x()),
                                              static_cast<float>(event->scenePos().y()));
        MediaCoordinates const media_coords(static_cast<float>(event->scenePos().x() / getXAspect()),
                                            static_cast<float>(event->scenePos().y() / getYAspect()));
        emit rightClickCanvas(canvas_coords);
        emit rightClickMediaCoords(media_coords);

    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}
void Media_Window::mouseReleaseEvent(QGraphicsSceneMouseEvent * event) {
    if (_debug_performance) {
        std::cout << "Mouse RELEASE - Button: " << (event->button() == Qt::LeftButton ? "LEFT" : "RIGHT")
                  << ", Was drawing: " << _is_drawing << ", Drawing points: " << _drawing_points.size() << std::endl;
    }

    if (event->button() == Qt::LeftButton) {
        // Always emit leftRelease signal
        emit leftRelease();

        // Only emit drawing-specific signal and reset drawing state when in drawing mode
        if (_is_drawing) {
            _is_drawing = false;
            emit leftReleaseDrawing();
            if (_debug_performance) {
                std::cout << "  Drawing finished - emitted leftReleaseDrawing signal" << std::endl;
            }
        }
    } else if (event->button() == Qt::RightButton) {
        // Always emit rightRelease signal
        emit rightRelease();

        // Only emit drawing-specific signal and reset drawing state when in drawing mode
        if (_is_drawing) {
            _is_drawing = false;
            emit rightReleaseDrawing();
        }
    }
    QGraphicsScene::mouseReleaseEvent(event);
}
void Media_Window::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
    static int move_count = 0;
    move_count++;

    auto pos = event->scenePos();

    _hover_position = pos;

    if (_is_drawing) {
        _drawing_points.push_back(pos);
        if (_debug_performance && move_count % 10 == 0) {// Only print every 10th move to avoid spam
            std::cout << "Mouse MOVE #" << move_count << " - Drawing: adding point (total: "
                      << _drawing_points.size() << ")" << std::endl;
        }
    } else if (_debug_performance && move_count % 50 == 0) {// Print every 50th move when not drawing
        std::cout << "Mouse MOVE #" << move_count << " - Hover only" << std::endl;
    }

    // Emit legacy signal
    emit mouseMove(event->scenePos().x(), event->scenePos().y());

    // Emit strong-typed coordinate signal
    CanvasCoordinates const canvas_coords(static_cast<float>(event->scenePos().x()),
                                          static_cast<float>(event->scenePos().y()));
    emit mouseMoveCanvas(canvas_coords);

    QGraphicsScene::mouseMoveEvent(event);
}

float Media_Window::getXAspect() const {

    std::string active_media_key;
    for (auto const & [config_key, config]: _media_configs) {
        if (config->is_visible) {
            active_media_key = config_key;
            break;
        }
    }
    if (active_media_key.empty()) {
        // No active media, return default aspect ratio
        return 1.0f;
    }

    auto _media = _data_manager->getData<MediaData>(active_media_key);
    if (!_media) {
        return 1.0f;// Default aspect ratio
    }

    float const scale_width = static_cast<float>(_canvasWidth) / static_cast<float>(_media->getWidth());

    return scale_width;
}

float Media_Window::getYAspect() const {

    std::string active_media_key;
    for (auto const & [config_key, config]: _media_configs) {
        if (config->is_visible) {
            active_media_key = config_key;
            break;
        }
    }
    if (active_media_key.empty()) {
        // No active media, return default aspect ratio
        return 1.0f;
    }

    auto _media = _data_manager->getData<MediaData>(active_media_key);
    if (!_media) {
        return 1.0f;// Default aspect ratio
    }

    float const scale_height = static_cast<float>(_canvasHeight) / static_cast<float>(_media->getHeight());

    return scale_height;
}

void Media_Window::_plotLineData() {
    auto const current_time = _data_manager->getCurrentTime();

    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

    auto xAspect = getXAspect();
    auto yAspect = getYAspect();

    for (auto const & [line_key, _line_config]: _line_configs) {

        if (!_line_config.get()->is_visible) continue;

        auto plot_color = plot_color_with_alpha(_line_config.get());

        auto line_timeframe_key = _data_manager->getTimeKey(line_key);
        auto line_timeframe = _data_manager->getTime(line_timeframe_key);

        auto line_data = _data_manager->getData<LineData>(line_key);
        auto lineData = line_data->getAtTime(TimeFrameIndex(current_time), video_timeframe.get(), line_timeframe.get());

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
            bool const is_selected = (_line_config.get()->selected_line_index == line_idx);

            // Use segment if enabled, otherwise use full line
            Line2D line_to_plot;
            if (_line_config.get()->show_segment) {
                float const start_percentage = static_cast<float>(_line_config.get()->segment_start_percentage) / 100.0f;
                float const end_percentage = static_cast<float>(_line_config.get()->segment_end_percentage) / 100.0f;
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
            QColor const dot_color = is_selected ? QColor(255, 0, 0) : plot_color;
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
                QBrush const emptyBrush(Qt::NoBrush);

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
                float const percentage = static_cast<float>(_line_config.get()->position_percentage) / 100.0f;
                Point2D<float> const marker_pos = get_position_at_percentage(line_to_plot, percentage);

                float const marker_x = marker_pos.x * xAspect;
                float const marker_y = marker_pos.y * yAspect;

                // Create a distinctive marker (filled circle with border)
                QPen markerPen(QColor(255, 255, 255));// White border
                markerPen.setWidth(2);
                QBrush const markerBrush(dot_color);// Same color as line but filled

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

    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

    for (auto const & [mask_key, _mask_config]: _mask_configs) {
        if (!_mask_config.get()->is_visible) continue;

        auto plot_color = plot_color_with_alpha(_mask_config.get());

        auto mask = _data_manager->getData<MaskData>(mask_key);
        auto image_size = mask->getImageSize();

        auto mask_timeframe_key = _data_manager->getTimeKey(mask_key);
        auto mask_timeframe = _data_manager->getTime(mask_timeframe_key);

        // Check for preview data first
        std::vector<Mask2D> maskData;
        std::vector<Mask2D> maskData2;

        if (_mask_preview_active && _preview_mask_data.count(mask_key) > 0) {
            // Use preview data
            maskData = _preview_mask_data[mask_key];
            maskData2.clear();// No time -1 data for preview
        } else {
            // Use original data
            maskData = mask->getAtTime(TimeFrameIndex(current_time), video_timeframe.get(), mask_timeframe.get());
            maskData2 = mask->getAtTime(TimeFrameIndex(-1));
        }

        _plotSingleMaskData(maskData, image_size, plot_color, _mask_config.get());
        _plotSingleMaskData(maskData2, image_size, plot_color, _mask_config.get());

        // Plot bounding boxes if enabled
        if (_mask_config.get()->show_bounding_box) {
            // Calculate scaling factors based on mask image size, not media aspect ratio
            float const xAspect = static_cast<float>(_canvasWidth) / static_cast<float>(image_size.width);
            float const yAspect = static_cast<float>(_canvasHeight) / static_cast<float>(image_size.height);

            // For current time masks
            for (auto const & single_mask: maskData) {
                if (!single_mask.empty()) {
                    auto bounding_box = get_bounding_box(single_mask);
                    auto min_point = bounding_box.first;
                    auto max_point = bounding_box.second;

                    // Scale coordinates to canvas using mask image size
                    float const min_x = static_cast<float>(min_point.x) * xAspect;
                    float const min_y = static_cast<float>(min_point.y) * yAspect;
                    float const max_x = static_cast<float>(max_point.x) * xAspect;
                    float const max_y = static_cast<float>(max_point.y) * yAspect;

                    // Draw bounding box rectangle (no fill, just outline)
                    QPen boundingBoxPen(plot_color);
                    boundingBoxPen.setWidth(2);
                    QBrush const emptyBrush(Qt::NoBrush);

                    auto boundingBoxRect = addRect(min_x, min_y, max_x - min_x, max_y - min_y,
                                                   boundingBoxPen, emptyBrush);
                    _mask_bounding_boxes.append(boundingBoxRect);
                }
            }

            // For time -1 masks
            for (auto const & single_mask: maskData2) {
                if (!single_mask.empty()) {
                    auto bounding_box = get_bounding_box(single_mask);
                    auto min_point = bounding_box.first;
                    auto max_point = bounding_box.second;

                    // Scale coordinates to canvas using mask image size
                    float const min_x = static_cast<float>(min_point.x) * xAspect;
                    float const min_y = static_cast<float>(min_point.y) * yAspect;
                    float const max_x = static_cast<float>(max_point.x) * xAspect;
                    float const max_y = static_cast<float>(max_point.y) * yAspect;

                    // Draw bounding box rectangle (no fill, just outline)
                    QPen boundingBoxPen(plot_color);
                    boundingBoxPen.setWidth(2);
                    QBrush const emptyBrush(Qt::NoBrush);

                    auto boundingBoxRect = addRect(min_x, min_y, max_x - min_x, max_y - min_y,
                                                   boundingBoxPen, emptyBrush);
                    _mask_bounding_boxes.append(boundingBoxRect);
                }
            }
        }

        // Plot outlines if enabled
        if (_mask_config.get()->show_outline) {
            // Create a slightly darker color for outlines
            QRgb const outline_color = plot_color;

            // For current time masks
            for (auto const & single_mask: maskData) {
                if (!single_mask.empty()) {
                    // Generate outline mask with thickness of 2 pixels
                    auto outline_mask = generate_outline_mask(single_mask, 2, image_size.width, image_size.height);

                    if (!outline_mask.empty()) {
                        // Plot the outline mask using the same approach as regular masks
                        _plotSingleMaskData({outline_mask}, image_size, outline_color, _mask_config.get());
                    }
                }
            }

            // For time -1 masks
            for (auto const & single_mask: maskData2) {
                if (!single_mask.empty()) {
                    // Generate outline mask with thickness of 2 pixels
                    auto outline_mask = generate_outline_mask(single_mask, 2, image_size.width, image_size.height);

                    if (!outline_mask.empty()) {
                        // Plot the outline mask using the same approach as regular masks
                        _plotSingleMaskData({outline_mask}, image_size, outline_color, _mask_config.get());
                    }
                }
            }
        }
    }
}

void Media_Window::_plotSingleMaskData(std::vector<Mask2D> const & maskData, ImageSize mask_size, QRgb plot_color, MaskDisplayOptions const * mask_config) {
    // Skip transparency masks as they are handled at the media level
    if (mask_config && mask_config->use_as_transparency) {
        return;
    }

    for (auto const & single_mask: maskData) {
        // Normal mode: overlay mask on top of media
        QImage unscaled_mask_image(mask_size.width, mask_size.height, QImage::Format::Format_ARGB32);
        unscaled_mask_image.fill(0);

        for (auto const point: single_mask) {
            unscaled_mask_image.setPixel(
                    QPoint(static_cast<int>(point.x), static_cast<int>(point.y)),
                    plot_color);
        }

        auto scaled_mask_image = unscaled_mask_image.scaled(_canvasWidth, _canvasHeight);
        auto maskPixmap = addPixmap(QPixmap::fromImage(scaled_mask_image));
        _masks.append(maskPixmap);
    }
}

QImage Media_Window::_applyTransparencyMasks(QImage const & media_image) {
    std::cout << "Applying transparency masks..." << std::endl;

    std::cout << "Media image size: " << media_image.width() << "x" << media_image.height() << std::endl;
    std::cout << "Canvas dimensions: " << _canvasWidth << "x" << _canvasHeight << std::endl;

    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

    QImage final_image = media_image;

    int transparency_mask_count = 0;
    int const total_mask_points = 0;

    // Process all transparency masks
    for (auto const & [mask_key, mask_config]: _mask_configs) {
        if (!mask_config->is_visible || !mask_config->use_as_transparency) {
            continue;
        }

        transparency_mask_count++;
        std::cout << "Processing transparency mask: " << mask_key << std::endl;

        auto mask_data = _data_manager->getData<MaskData>(mask_key);
        auto image_size = mask_data->getImageSize();

        auto mask_timeframe_key = _data_manager->getTimeKey(mask_key);
        auto mask_timeframe = _data_manager->getTime(mask_timeframe_key);

        std::cout << "Mask image size: " << image_size.width << "x" << image_size.height << std::endl;

        auto const current_time = _data_manager->getCurrentTime();
        auto maskData = mask_data->getAtTime(TimeFrameIndex(current_time), video_timeframe.get(), mask_timeframe.get());

        std::cout << "Mask data size: " << maskData.size() << std::endl;

        // Calculate scaling factors
        float const xAspect = static_cast<float>(_canvasWidth) / static_cast<float>(image_size.width);
        float const yAspect = static_cast<float>(_canvasHeight) / static_cast<float>(image_size.height);

        std::cout << "Scaling factors: x=" << xAspect << ", y=" << yAspect << std::endl;

        QImage unscaled_mask_image(image_size.width, image_size.height, QImage::Format::Format_ARGB32);
        unscaled_mask_image.fill(0);
        // Add mask points to combined mask
        for (auto const & single_mask: maskData) {
            for (auto const point: single_mask) {
                unscaled_mask_image.setPixel(
                        QPoint(static_cast<int>(point.x), static_cast<int>(point.y)),
                        qRgba(255, 255, 255, 255));
            }
        }

        QImage const scaled_mask_image = unscaled_mask_image.scaled(_canvasWidth, _canvasHeight);
        // I want to copy final_image where scaled_mask_image is white, and keep the rest of the image the same
        for (int y = 0; y < _canvasHeight; ++y) {
            for (int x = 0; x < _canvasWidth; ++x) {
                if (scaled_mask_image.pixel(x, y) == qRgba(255, 255, 255, 255)) {
                    final_image.setPixel(x, y, final_image.pixel(x, y));
                } else {
                    final_image.setPixel(x, y, qRgba(0, 0, 0, 255));
                }
            }
        }
    }


    return final_image;
}

void Media_Window::_plotPointData() {

    auto const current_time = TimeFrameIndex(_data_manager->getCurrentTime());
    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

    if (!video_timeframe) {
        std::cerr << "Error: Could not get video timeframe 'time' for point conversion" << std::endl;
        return;
    }

    for (auto const & [point_key, _point_config]: _point_configs) {
        if (!_point_config.get()->is_visible) continue;

        auto plot_color = plot_color_with_alpha(_point_config.get());

        auto point = _data_manager->getData<PointData>(point_key);

        auto point_timeframe_key = _data_manager->getTimeKey(point_key);
        if (point_timeframe_key.empty()) {
            std::cerr << "Error: No timeframe found for point data: " << point_key << std::endl;
            continue;
        }

        auto point_timeframe = _data_manager->getTime(point_timeframe_key);

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

        auto pointData = point->getAtTime(current_time, video_timeframe.get(), point_timeframe.get());

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
                    QBrush const brush(plot_color);
                    auto ellipse = addEllipse(x_pos - point_size / 2, y_pos - point_size / 2,
                                              point_size, point_size, pen, brush);
                    _points.append(ellipse);
                    break;
                }
                case PointMarkerShape::Square: {
                    QPen pen(plot_color);
                    pen.setWidth(2);
                    QBrush const brush(plot_color);
                    auto rect = addRect(x_pos - point_size / 2, y_pos - point_size / 2,
                                        point_size, point_size, pen, brush);
                    _points.append(rect);
                    break;
                }
                case PointMarkerShape::Triangle: {
                    QPen pen(plot_color);
                    pen.setWidth(2);
                    QBrush const brush(plot_color);

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
    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

    for (auto const & [key, _interval_config]: _interval_configs) {
        if (!_interval_config.get()->is_visible) continue;

        // Only render if using Box plotting style
        if (_interval_config.get()->plotting_style != IntervalPlottingStyle::Box) continue;

        auto plot_color = plot_color_with_alpha(_interval_config.get());

        auto interval_series = _data_manager->getData<DigitalIntervalSeries>(key);

        // Get the timeframes for conversion
        auto interval_timeframe_key = _data_manager->getTimeKey(key);
        if (interval_timeframe_key.empty()) {
            std::cerr << "Error: No timeframe found for digital interval series: " << key << std::endl;
            continue;
        }

        auto interval_timeframe = _data_manager->getTime(interval_timeframe_key);

        if (!video_timeframe) {
            std::cerr << "Error: Could not get video timeframe 'time' for interval conversion" << std::endl;
            continue;
        }
        if (!interval_timeframe) {
            std::cerr << "Error: Could not get interval timeframe '" << interval_timeframe_key
                      << "' for series: " << key << std::endl;
            continue;
        }

        bool const needs_conversion = _needsTimeFrameConversion(video_timeframe, interval_timeframe);

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
                int const video_time_value = video_timeframe->getTimeAtIndex(TimeFrameIndex(video_time));

                // 2. Convert time value to index in interval series timeframe
                query_time = interval_timeframe->getIndexAtTime(static_cast<float>(video_time_value)).getValue();
            }

            bool const event_present = interval_series->isEventAtTime(TimeFrameIndex(query_time));

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

        // Get the timeframes for conversion
        auto interval_timeframe_key = _data_manager->getTimeKey(key);
        if (interval_timeframe_key.empty()) {
            std::cerr << "Error: No timeframe found for digital interval series: " << key << std::endl;
            continue;
        }

        auto video_timeframe = _data_manager->getTime(TimeKey("time"));
        auto interval_timeframe = _data_manager->getTime(interval_timeframe_key);

        if (!video_timeframe) {
            std::cerr << "Error: Could not get video timeframe 'time' for interval conversion" << std::endl;
            continue;
        }
        if (!interval_timeframe) {
            std::cerr << "Error: Could not get interval timeframe '" << interval_timeframe_key
                      << "' for series: " << key << std::endl;
            continue;
        }

        bool const needs_conversion = _needsTimeFrameConversion(video_timeframe, interval_timeframe);

        // Check if an interval is present at the current frame
        bool interval_present = false;
        if (needs_conversion) {
            // Convert current video time to interval timeframe
            auto video_time = video_timeframe->getTimeAtIndex(TimeFrameIndex(current_time));
            auto interval_index = interval_timeframe->getIndexAtTime(video_time);
            interval_present = interval_series->isEventAtTime(interval_index);
        } else {
            // Direct comparison (no timeframe conversion needed)
            interval_present = interval_series->isEventAtTime(TimeFrameIndex(current_time));
        }

        // If an interval is present, draw a border around the entire image
        if (interval_present) {
            auto plot_color = plot_color_with_alpha(_interval_config.get());

            // Get border thickness from config
            int const thickness = _interval_config->border_thickness;

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

        auto tensor_slice = tensor_data->getChannelSlice(TimeFrameIndex(current_time), config->display_channel);

        // Create a QImage from the tensor data
        QImage tensor_image(static_cast<int>(tensor_shape[1]), static_cast<int>(tensor_shape[0]), QImage::Format::Format_ARGB32);
        for (size_t y = 0; y < tensor_shape[0]; ++y) {
            for (size_t x = 0; x < tensor_shape[1]; ++x) {
                float const value = tensor_slice[y * tensor_shape[1] + x];
                //int const pixel_value = static_cast<int>(value * 255);// Assuming the tensor values are normalized between 0 and 1

                // Use the config color with alpha
                QColor const color(QString::fromStdString(config->hex_color));
                int const alpha = std::lround(config->alpha * 255.0f * (value > 0 ? 1.0f : 0.0f));
                QRgb const rgb = qRgba(color.red(), color.green(), color.blue(), alpha);

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
        if (_debug_performance) {
            std::cout << "Hover circle enabled" << std::endl;
        }

        // Create the hover circle item if it doesn't exist
        if (!_hover_circle_item) {
            QPen circlePen(Qt::red);
            circlePen.setWidth(2);
            _hover_circle_item = addEllipse(0, 0, _hover_circle_radius * 2, _hover_circle_radius * 2, circlePen);
            _hover_circle_item->setVisible(false);// Initially hidden until mouse moves
            // DO NOT add to _points vector - hover circle is managed separately
            if (_debug_performance) {
                std::cout << "  Created new hover circle item" << std::endl;
            }
        }

        // Connect mouse move to efficient hover circle update instead of full canvas update
        connect(this, &Media_Window::mouseMove, this, &Media_Window::_updateHoverCirclePosition);
    } else {
        if (_debug_performance) {
            std::cout << "Hover circle disabled" << std::endl;
        }

        // Remove the hover circle item
        if (_hover_circle_item) {
            removeItem(_hover_circle_item);
            delete _hover_circle_item;
            _hover_circle_item = nullptr;
            if (_debug_performance) {
                std::cout << "  Deleted hover circle item" << std::endl;
            }
        }

        // Disconnect the mouse move signal
        disconnect(this, &Media_Window::mouseMove, this, &Media_Window::_updateHoverCirclePosition);
    }
}

void Media_Window::setHoverCircleRadius(int radius) {
    _hover_circle_radius = radius;

    // Update the existing hover circle item if it exists
    if (_hover_circle_item && _show_hover_circle) {
        qreal const x = _hover_position.x() - _hover_circle_radius;
        qreal const y = _hover_position.y() - _hover_circle_radius;
        _hover_circle_item->setRect(x, y, _hover_circle_radius * 2, _hover_circle_radius * 2);
    }
}

void Media_Window::_updateHoverCirclePosition() {
    static int call_count = 0;
    call_count++;

    if (_hover_circle_item && _show_hover_circle) {
        // Update the position of the existing hover circle item
        qreal const x = _hover_position.x() - _hover_circle_radius;
        qreal const y = _hover_position.y() - _hover_circle_radius;
        _hover_circle_item->setRect(x, y, _hover_circle_radius * 2, _hover_circle_radius * 2);
        _hover_circle_item->setVisible(true);

        if (_debug_performance) {
            std::cout << "Hover circle updated (call #" << call_count << ") at ("
                      << _hover_position.x() << ", " << _hover_position.y() << ")" << std::endl;
        }
    } else {
        if (_debug_performance) {
            std::cout << "Hover circle update skipped (call #" << call_count << ") - item: "
                      << (_hover_circle_item ? "exists" : "null") << ", show: " << _show_hover_circle << std::endl;
        }
    }
}

void Media_Window::_addRemoveData() {
    //New data key was added. This is where we may want to repopulate a custom table
}

bool Media_Window::_needsTimeFrameConversion(std::shared_ptr<TimeFrame> video_timeframe,
                                             std::shared_ptr<TimeFrame> const & interval_timeframe) {
    // If either timeframe is null, no conversion is possible/needed
    if (!video_timeframe || !interval_timeframe) {
        return false;
    }

    // Conversion is needed if the timeframes are different objects
    return video_timeframe.get() != interval_timeframe.get();
}


QRgb plot_color_with_alpha(BaseDisplayOptions const * opts) {
    auto color = QColor(QString::fromStdString(opts->hex_color));
    auto output_color = qRgba(color.red(), color.green(), color.blue(), std::lround(opts->alpha * 255.0f));

    return output_color;
}

bool Media_Window::hasPreviewMaskData(std::string const & mask_key) const {
    return _mask_preview_active && _preview_mask_data.count(mask_key) > 0;
}

std::vector<Mask2D> Media_Window::getPreviewMaskData(std::string const & mask_key) const {

    if (hasPreviewMaskData(mask_key)) {
        return _preview_mask_data.at(mask_key);
    }
    return {};
}

void Media_Window::setPreviewMaskData(std::string const & mask_key,
                                      std::vector<std::vector<Point2D<uint32_t>>> const & preview_data,
                                      bool active) {
    if (active) {
        _preview_mask_data[mask_key] = preview_data;
        _mask_preview_active = true;
    } else {
        _preview_mask_data.erase(mask_key);
        _mask_preview_active = !_preview_mask_data.empty();
    }
}
