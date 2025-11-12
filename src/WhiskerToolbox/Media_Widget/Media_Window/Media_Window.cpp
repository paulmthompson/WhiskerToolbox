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
#include "GroupManagementWidget/GroupManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include <QAction>
#include <QElapsedTimer>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
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

    _createContextMenu();
}

Media_Window::~Media_Window() {
    // Clean up context menu
    if (_context_menu) {
        delete _context_menu;
        _context_menu = nullptr;
    }

    // Clear temporary line items before clearing the scene
    clearTemporaryLine();

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

void Media_Window::setGroupManager(GroupManager * group_manager) {
    // Disconnect from previous group manager if any
    if (_group_manager) {
        disconnect(_group_manager, nullptr, this, nullptr);
    }
    
    _group_manager = group_manager;
    
    // Connect to new group manager signals if available
    if (_group_manager) {
        connect(_group_manager, &GroupManager::groupCreated, this, &Media_Window::onGroupChanged);
        connect(_group_manager, &GroupManager::groupRemoved, this, &Media_Window::onGroupChanged);
        connect(_group_manager, &GroupManager::groupModified, this, &Media_Window::onGroupChanged);
    }
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
        } else if (_group_selection_enabled) {
            // Handle selection on left click (when not in drawing mode and group selection is enabled)
            std::string data_key, data_type;
            EntityId entity_id = _findEntityAtPosition(event->scenePos(), data_key, data_type);
            
            if (entity_id != EntityId(0)) {
                // Use group-based selection for all entity types
                // Check if Ctrl is held for multi-selection
                if (event->modifiers() & Qt::ControlModifier) {
                    if (_selected_entities.count(entity_id)) {
                        _selected_entities.erase(entity_id);
                    } else {
                        _selected_entities.insert(entity_id);
                        _selected_data_key = data_key;
                        _selected_data_type = data_type;
                    }
                } else {
                    // Single selection
                    _selected_entities.clear();
                    _selected_entities.insert(entity_id);
                    _selected_data_key = data_key;
                    _selected_data_type = data_type;
                }
                UpdateCanvas(); // Refresh to show selection
            } else if (!(event->modifiers() & Qt::ControlModifier)) {
                // Clear selection if clicking on empty area without Ctrl
                clearAllSelections();
            }
        }

        // Emit legacy signals (qreal values)
        emit leftClick(event->scenePos().x(), event->scenePos().y());
        emit leftClickMedia(
                event->scenePos().x() / getXAspect(),
                event->scenePos().y() / getYAspect());
        
        // Emit media click signal with modifier information
        emit leftClickMediaWithEvent(
                event->scenePos().x() / getXAspect(),
                event->scenePos().y() / getYAspect(),
                event->modifiers());

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

void Media_Window::contextMenuEvent(QGraphicsSceneContextMenuEvent * event) {
    // Only show context menu if we have selections and a group manager
    if (!hasSelections() || !_group_manager) {
        QGraphicsScene::contextMenuEvent(event);
        return;
    }

    _updateContextMenuActions();
    _showContextMenu(event->screenPos());
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

        auto line_data = _data_manager->getData<LineData>(line_key);
        auto lineData = line_data->getAtTime(TimeFrameIndex(current_time), *video_timeframe);
        auto entityIds = line_data->getEntityIdsAtTime(TimeFrameIndex(current_time), *video_timeframe);

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

        // Ensure we have matching line data and entity IDs
        size_t line_count = std::min(lineData.size(), entityIds.size());

        for (int line_idx = 0; line_idx < static_cast<int>(line_count); ++line_idx) {
            auto const & single_line = lineData[line_idx];
            EntityId entity_id = static_cast<size_t>(line_idx) < entityIds.size() ? entityIds[line_idx] : EntityId(0);

            if (single_line.empty()) {
                continue;
            }

            // Check if the entity's group is visible
            if (!_isEntityGroupVisible(entity_id)) {
                continue; // Skip rendering this entity if its group is not visible
            }

            // Use group-aware color if available, otherwise use default plot color
            QColor line_color = _getGroupAwareColor(entity_id, QColor::fromRgba(plot_color));

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

            // Create pen with group-aware color and configurable thickness
            QPen linePen;
            linePen.setColor(line_color);
            linePen.setWidth(_line_config.get()->line_thickness);

            auto linePath = addPath(path, linePen);
            _line_paths.append(linePath);

            // Add dot at line base (always filled) - use group-aware color
            QColor const dot_color = line_color;
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
    // Note: MaskData does not currently support EntityIds for group-aware coloring
    // This would need to be implemented similar to PointData and LineData
    auto const current_time = _data_manager->getCurrentTime();

    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

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
            auto mask_data_view = mask->getAtTime(TimeFrameIndex(current_time), *video_timeframe);
            maskData = std::vector<Mask2D>(mask_data_view.begin(), mask_data_view.end());
            auto mask_data_view2 = mask->getAtTime(TimeFrameIndex(-1));
            maskData2 = std::vector<Mask2D>(mask_data_view2.begin(), mask_data_view2.end());
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

        std::cout << "Mask image size: " << image_size.width << "x" << image_size.height << std::endl;

        auto const current_time = _data_manager->getCurrentTime();
        auto maskData = mask_data->getAtTime(TimeFrameIndex(current_time), *video_timeframe);

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

        auto pointData = point->getAtTime(current_time, *video_timeframe);
        auto entityIds = point->getEntityIdsAtTime(current_time);

        // Get configurable point size
        float const point_size = static_cast<float>(_point_config.get()->point_size);

        // Ensure we have matching point data and entity IDs
        size_t count = std::min(pointData.size(), entityIds.size());
        
        for (size_t i = 0; i < count; ++i) {
            auto const & single_point = pointData[i];
            EntityId entity_id = entityIds[i];
            
            // Check if the entity's group is visible
            if (!_isEntityGroupVisible(entity_id)) {
                continue; // Skip rendering this entity if its group is not visible
            }
            
            float const x_pos = single_point.x * xAspect;
            float const y_pos = single_point.y * yAspect;

            // Use group-aware color if available, otherwise use default plot color
            QColor point_color = _getGroupAwareColor(entity_id, QColor::fromRgba(plot_color));
            
            // Check if this point is selected to add highlight
            bool is_selected = _selected_entities.count(entity_id) > 0;
            
            // Add selection highlight if point is selected
            if (is_selected) {
                QPen highlight_pen(Qt::yellow);
                highlight_pen.setWidth(4);
                QBrush highlight_brush(Qt::transparent);
                auto highlight_circle = addEllipse(x_pos - point_size, y_pos - point_size,
                                                   point_size * 2, point_size * 2, 
                                                   highlight_pen, highlight_brush);
                _points.append(highlight_circle);
            }

            // Create the appropriate marker shape based on configuration
            switch (_point_config.get()->marker_shape) {
                case PointMarkerShape::Circle: {
                    QPen pen(point_color);
                    pen.setWidth(2);
                    QBrush const brush(point_color);
                    auto ellipse = addEllipse(x_pos - point_size / 2, y_pos - point_size / 2,
                                              point_size, point_size, pen, brush);
                    _points.append(ellipse);
                    break;
                }
                case PointMarkerShape::Square: {
                    QPen pen(point_color);
                    pen.setWidth(2);
                    QBrush const brush(point_color);
                    auto rect = addRect(x_pos - point_size / 2, y_pos - point_size / 2,
                                        point_size, point_size, pen, brush);
                    _points.append(rect);
                    break;
                }
                case PointMarkerShape::Triangle: {
                    QPen pen(point_color);
                    pen.setWidth(2);
                    QBrush const brush(point_color);

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
                    QPen pen(point_color);
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
                    QPen pen(point_color);
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
                    QPen pen(point_color);
                    pen.setWidth(2);
                    QBrush brush(point_color);

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

void Media_Window::setShowTemporaryLine(bool show) {
    _show_temporary_line = show;
    if (!_show_temporary_line) {
        clearTemporaryLine();
    }
}

void Media_Window::updateTemporaryLine(std::vector<Point2D<float>> const & points, std::string const & line_key) {
    if (!_show_temporary_line || points.empty()) {
        return;
    }

    // Clear existing temporary line
    clearTemporaryLine();

    // Use the same aspect ratio calculation as the target line data
    auto xAspect = getXAspect();
    auto yAspect = getYAspect();
    
    // If we have a line key, use the same image size scaling as that line data
    if (!line_key.empty()) {
        auto line_data = _data_manager->getData<LineData>(line_key);
        if (line_data) {
            auto image_size = line_data->getImageSize();
            
        }
    }

    if (points.size() < 2) {
        // If only one point, just show a marker
        // Convert media coordinates to canvas coordinates
        float x = points[0].x * xAspect;
        float y = points[0].y * yAspect;
        
        QPen pointPen(Qt::yellow);
        pointPen.setWidth(2);
        QBrush pointBrush(Qt::yellow);
        
        auto pointItem = addEllipse(x - 3, y - 3, 6, 6, pointPen, pointBrush);
        _temporary_line_points.push_back(pointItem);
        return;
    }

    // Create path for the line
    QPainterPath path;


    // Move to first point (convert media coordinates to canvas coordinates)
    path.moveTo(QPointF(points[0].x * xAspect, points[0].y * yAspect));

    // Add lines to subsequent points
    for (size_t i = 1; i < points.size(); ++i) {
        path.lineTo(QPointF(points[i].x * xAspect, points[i].y * yAspect));
    }

    // Create the line path item
    QPen linePen(Qt::yellow);
    linePen.setWidth(2);
    linePen.setStyle(Qt::DashLine); // Dashed line to distinguish from permanent lines
    
    _temporary_line_item = addPath(path, linePen);

    // Add point markers
    QPen pointPen(Qt::yellow);
    pointPen.setWidth(1);
    QBrush pointBrush(Qt::NoBrush); // Open circles

    for (size_t i = 0; i < points.size(); ++i) {
        // Convert media coordinates to canvas coordinates
        float x = points[i].x * xAspect;
        float y = points[i].y * yAspect;
        
        auto pointItem = addEllipse(x - 2.5, y - 2.5, 5, 5, pointPen, pointBrush);
        _temporary_line_points.push_back(pointItem);
    }
}

void Media_Window::clearTemporaryLine() {
    // Remove and delete the temporary line path
    if (_temporary_line_item) {
        removeItem(_temporary_line_item);
        delete _temporary_line_item;
        _temporary_line_item = nullptr;
    }

    // Remove and delete all temporary line point markers
    for (auto pointItem : _temporary_line_points) {
        if (pointItem) {
            removeItem(pointItem);
            delete pointItem;
        }
    }
    _temporary_line_points.clear();
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
                                      std::vector<Mask2D> const & preview_data,
                                      bool active) {
    if (active) {
        _preview_mask_data[mask_key] = preview_data;
        _mask_preview_active = true;
    } else {
        _preview_mask_data.erase(mask_key);
        _mask_preview_active = !_preview_mask_data.empty();
    }
}

void Media_Window::onGroupChanged() {
    // Update the canvas when group assignments or properties change
    UpdateCanvas();
}

QColor Media_Window::_getGroupAwareColor(EntityId entity_id, QColor const & default_color) const {
    // Handle selection highlighting first
    if (_selected_entities.count(entity_id) > 0) {
        return QColor(255, 255, 0); // Bright yellow for selected entities
    }
    
    if (!_group_manager || entity_id == EntityId(0)) {
        return default_color;
    }
    
    return _group_manager->getEntityColor(entity_id, default_color);
}

bool Media_Window::_isEntityGroupVisible(EntityId entity_id) const {
    if (!_group_manager || entity_id == EntityId(0)) {
        return true; // Entities not in a group or without group manager are always visible
    }
    
    return _group_manager->isEntityGroupVisible(entity_id);
}

QRgb Media_Window::_getGroupAwareColorRgb(EntityId entity_id, QRgb default_color) const {
    // Handle selection highlighting first
    if (_selected_entities.count(entity_id) > 0) {
        return qRgba(255, 255, 0, 255); // Bright yellow for selected entities
    }
    
    if (!_group_manager || entity_id == EntityId(0)) {
        return default_color;
    }
    
    QColor group_color = _group_manager->getEntityColor(entity_id, QColor::fromRgba(default_color));
    return group_color.rgba();
}

// ===== Selection and Context Menu Implementation =====

void Media_Window::clearAllSelections() {
    if (!_selected_entities.empty()) {
        _selected_entities.clear();
        _selected_data_key.clear();
        _selected_data_type.clear();
        UpdateCanvas(); // Refresh to remove selection highlights
    }
}

bool Media_Window::hasSelections() const {
    return !_selected_entities.empty();
}

std::unordered_set<EntityId> Media_Window::getSelectedEntities() const {
    return _selected_entities;
}

void Media_Window::setGroupSelectionEnabled(bool enabled) {
    _group_selection_enabled = enabled;
    if (!enabled) {
        // Clear any existing selections when disabling group selection
        clearAllSelections();
    }
}

bool Media_Window::isGroupSelectionEnabled() const {
    return _group_selection_enabled;
}

EntityId Media_Window::findPointAtPosition(QPointF const & scene_pos, std::string const & point_key) {
    return _findPointAtPosition(scene_pos, point_key);
}

EntityId Media_Window::findEntityAtPosition(QPointF const & scene_pos, std::string & data_key, std::string & data_type) {
    return _findEntityAtPosition(scene_pos, data_key, data_type);
}

void Media_Window::selectEntity(EntityId entity_id, std::string const & data_key, std::string const & data_type) {
    _selected_entities.clear();
    _selected_entities.insert(entity_id);
    _selected_data_key = data_key;
    _selected_data_type = data_type;
    UpdateCanvas(); // Refresh to show selection
}

EntityId Media_Window::_findEntityAtPosition(QPointF const & scene_pos, std::string & data_key, std::string & data_type) {
    // Convert scene coordinates to media coordinates
    float x_media = static_cast<float>(scene_pos.x() / getXAspect());
    float y_media = static_cast<float>(scene_pos.y() / getYAspect());

    // Search through lines first (as they're typically most precise)
    for (auto const & [key, config] : _line_configs) {
        if (config->is_visible) {
            EntityId entity_id = _findLineAtPosition(scene_pos, key);
            if (entity_id != EntityId(0)) {
                data_key = key;
                data_type = "line";
                return entity_id;
            }
        }
    }

    // Then search points
    for (auto const & [key, config] : _point_configs) {
        if (config->is_visible) {
            EntityId entity_id = _findPointAtPosition(scene_pos, key);
            if (entity_id != EntityId(0)) {
                data_key = key;
                data_type = "point";
                return entity_id;
            }
        }
    }

    // Finally search masks (usually less precise)
    for (auto const & [key, config] : _mask_configs) {
        if (config->is_visible) {
            EntityId entity_id = _findMaskAtPosition(scene_pos, key);
            if (entity_id != EntityId(0)) {
                data_key = key;
                data_type = "mask";
                return entity_id;
            }
        }
    }

    return EntityId(0); // No entity found
}

EntityId Media_Window::_findLineAtPosition(QPointF const & scene_pos, std::string const & line_key) {
    auto line_data = _data_manager->getData<LineData>(line_key);
    if (!line_data) {
        return EntityId(0);
    }

    // Use the same timeframe conversion as rendering to ensure indices match
    auto const current_time = _data_manager->getCurrentTime();
    auto video_timeframe = _data_manager->getTime(TimeKey("time"));
    auto const & lines = line_data->getAtTime(TimeFrameIndex(current_time), *video_timeframe);
    auto const & entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex(current_time), *video_timeframe);

    if (lines.size() != entity_ids.size()) {
        return EntityId(0);
    }

    // Match scaling used in _plotLineData for this line dataset
    float xAspect = getXAspect();
    float yAspect = getYAspect();
    auto const image_size = line_data->getImageSize();
    if (image_size.height != -1) {
        auto const line_height = static_cast<float>(image_size.height);
        yAspect = static_cast<float>(_canvasHeight) / line_height;
    }
    if (image_size.width != -1) {
        auto const line_width = static_cast<float>(image_size.width);
        xAspect = static_cast<float>(_canvasWidth) / line_width;
    }

    float const threshold = 10.0f; // pixels

    // Find the nearest line (minimum distance) rather than returning on first hit
    float best_dist = std::numeric_limits<float>::max();
    std::size_t best_index = static_cast<std::size_t>(-1);

    for (std::size_t i = 0; i < lines.size(); ++i) {
        auto const & line = lines[i];
        if (line.size() < 2) {
            continue;
        }

        for (std::size_t j = 0; j + 1 < line.size(); ++j) {
            auto const & p1 = line[j];
            auto const & p2 = line[j + 1];

            // Convert line points to scene coordinates (same as rendering)
            float const x1_scene = p1.x * xAspect;
            float const y1_scene = p1.y * yAspect;
            float const x2_scene = p2.x * xAspect;
            float const y2_scene = p2.y * yAspect;

            float const dist = _calculateDistanceToLineSegment(
                    scene_pos.x(), scene_pos.y(),
                    x1_scene, y1_scene, x2_scene, y2_scene);

            if (dist < best_dist) {
                best_dist = dist;
                best_index = i;
            }
        }
    }

    if (best_index != static_cast<std::size_t>(-1) && best_dist <= threshold) {
        return entity_ids[best_index];
    }

    return EntityId(0);
}

EntityId Media_Window::_findPointAtPosition(QPointF const & scene_pos, std::string const & point_key) {
    auto point_data = _data_manager->getData<PointData>(point_key);
    if (!point_data) {
        return EntityId(0);
    }

    auto current_time = _data_manager->getCurrentTime();
    auto const & points = point_data->getAtTime(TimeFrameIndex(current_time));
    auto const & entity_ids = point_data->getEntityIdsAtTime(TimeFrameIndex(current_time));

    if (points.size() != entity_ids.size()) {
        return EntityId(0);
    }

    float const threshold = 15.0f; // pixels

    for (size_t i = 0; i < points.size(); ++i) {
        auto const & point = points[i];
        
        // Convert point to scene coordinates
        float x_scene = point.x * getXAspect();
        float y_scene = point.y * getYAspect();
        
        // Calculate distance
        float dx = scene_pos.x() - x_scene;
        float dy = scene_pos.y() - y_scene;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        if (distance <= threshold) {
            return entity_ids[i];
        }
    }

    return EntityId(0);
}

EntityId Media_Window::_findMaskAtPosition(QPointF const & scene_pos, std::string const & mask_key) {
    auto mask_data = _data_manager->getData<MaskData>(mask_key);
    if (!mask_data) {
        return EntityId(0);
    }

    auto current_time = _data_manager->getCurrentTime();
    auto const & masks = mask_data->getAtTime(TimeFrameIndex(current_time));

    // MaskData doesn't currently support EntityIds, so we'll use position-based indices for now
    // This is a simplified implementation that can be improved when MaskData gets EntityId support

    float x_media = static_cast<float>(scene_pos.x() / getXAspect());
    float y_media = static_cast<float>(scene_pos.y() / getYAspect());

    for (size_t i = 0; i < masks.size(); ++i) {
        auto const & mask = masks[i];
        
        // Check if the point is inside any of the mask's polygons
        for (auto const & point : mask) {
            // Simple bounding box check for now (could be improved with proper point-in-polygon)
            if (std::abs(static_cast<float>(point.x) - x_media) < 5.0f && 
                std::abs(static_cast<float>(point.y) - y_media) < 5.0f) {
                // Return a synthetic EntityId based on position and mask index
                // This is temporary until MaskData supports proper EntityIds
                return EntityId(1000000 + current_time * 1000 + i);
            }
        }
    }

    return EntityId(0);
}

void Media_Window::_createContextMenu() {
    _context_menu = new QMenu();
    
    // Create actions
    auto * create_group_action = new QAction("Create New Group", this);
    auto * ungroup_action = new QAction("Ungroup Selected", this);
    auto * clear_selection_action = new QAction("Clear Selection", this);
    
    // Add actions to menu
    _context_menu->addAction(create_group_action);
    _context_menu->addSeparator();
    _context_menu->addAction(ungroup_action);
    _context_menu->addSeparator();
    _context_menu->addAction(clear_selection_action);
    
    // Connect actions
    connect(create_group_action, &QAction::triggered, this, &Media_Window::onCreateNewGroup);
    connect(ungroup_action, &QAction::triggered, this, &Media_Window::onUngroupSelected);
    connect(clear_selection_action, &QAction::triggered, this, &Media_Window::onClearSelection);
}

void Media_Window::_showContextMenu(QPoint const & global_pos) {
    if (_context_menu) {
        _context_menu->popup(global_pos);
    }
}

void Media_Window::_updateContextMenuActions() {
    if (!_context_menu || !_group_manager) {
        return;
    }

    // Clear all dynamic actions by removing actions after the static ones
    // The static menu structure is: Create New Group, Separator, Ungroup Selected, Separator, Clear Selection
    // Everything after the second separator should be removed
    auto actions = _context_menu->actions();
    int separator_count = 0;
    QList<QAction*> actions_to_remove;
    
    for (QAction* action : actions) {
        if (action->isSeparator()) {
            separator_count++;
            if (separator_count > 2) {
                actions_to_remove.append(action);
            }
        } else if (separator_count >= 2) {
            // This is a dynamic action after the second separator
            actions_to_remove.append(action);
        }
    }
    
    // Remove and delete the dynamic actions
    for (QAction* action : actions_to_remove) {
        _context_menu->removeAction(action);
        action->deleteLater(); // Use deleteLater() for safer cleanup
    }

    // Add dynamic group assignment actions
    auto groups = _group_manager->getGroupsForContextMenu();
    if (!groups.empty()) {
        _context_menu->addSeparator();
        
        for (auto const & [group_id, group_name] : groups) {
            auto * assign_action = new QAction(QString("Assign to %1").arg(group_name), this);
            _context_menu->addAction(assign_action);
            
            connect(assign_action, &QAction::triggered, this, [this, group_id]() {
                onAssignToGroup(group_id);
            });
        }
    }
}

float Media_Window::_calculateDistanceToLineSegment(float px, float py, float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    
    if (dx == 0 && dy == 0) {
        // Point to point distance
        float dpx = px - x1;
        float dpy = py - y1;
        return std::sqrt(dpx * dpx + dpy * dpy);
    }
    
    float t = ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy);
    t = std::max(0.0f, std::min(1.0f, t));
    
    float projection_x = x1 + t * dx;
    float projection_y = y1 + t * dy;
    
    float dist_x = px - projection_x;
    float dist_y = py - projection_y;
    
    return std::sqrt(dist_x * dist_x + dist_y * dist_y);
}

// Context menu slot implementations
void Media_Window::onCreateNewGroup() {
    if (!_group_manager || _selected_entities.empty()) {
        return;
    }
    
    int group_id = _group_manager->createGroupWithEntities(_selected_entities);
    if (group_id != -1) {
        clearAllSelections();
    }
}

void Media_Window::onAssignToGroup(int group_id) {
    if (!_group_manager || _selected_entities.empty()) {
        return;
    }
    
    _group_manager->assignEntitiesToGroup(group_id, _selected_entities);
    clearAllSelections();
}

void Media_Window::onUngroupSelected() {
    if (!_group_manager || _selected_entities.empty()) {
        return;
    }
    
    _group_manager->ungroupEntities(_selected_entities);
    clearAllSelections();
}

void Media_Window::onClearSelection() {
    clearAllSelections();
}

void Media_Window::keyPressEvent(QKeyEvent * event) {
    // Handle keyboard shortcuts for group assignment (1-9)
    if (!_group_manager || _selected_entities.empty()) {
        QGraphicsScene::keyPressEvent(event);
        return;
    }

    // Check if the pressed key is a digit from 1 to 9
    int key_value = event->key();
    if (key_value >= Qt::Key_1 && key_value <= Qt::Key_9) {
        // Convert key to group number (Qt::Key_1 = 49, so subtract 48 to get 1-9)
        int group_number = key_value - Qt::Key_0;

        // Get available groups from the group manager
        auto groups = _group_manager->getGroupsForContextMenu();

        // Find the group with the matching number in the display order
        if (group_number <= static_cast<int>(groups.size())) {
            // Groups are returned in order, so we can use index-based access
            auto it = groups.begin();
            std::advance(it, group_number - 1);
            int group_id = it->first;

            // Assign selected entities to the group
            onAssignToGroup(group_id);
            event->accept();
            return;
        }
    }

    // If not handled, pass to parent
    QGraphicsScene::keyPressEvent(event);
}

