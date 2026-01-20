#include "MediaMask_Widget.hpp"
#include "ui_MediaMask_Widget.h"

#include "Collapsible_Widget/Section.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include "MaskDilationWidget/MaskDilationWidget.hpp"
#include "Media_Widget/Media_Window/Media_Window.hpp"
#include "SelectionWidgets/MaskBrushSelectionWidget.hpp"
#include "SelectionWidgets/MaskNoneSelectionWidget.hpp"

#include <QLabel>
#include <QVBoxLayout>

#include <cmath>
#include <iostream>
#include <set>

MediaMask_Widget::MediaMask_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaMask_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene} {
    ui->setupUi(this);

    // Set size policy for proper resizing
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Setup selection modes
    _selection_modes["(None)"] = Selection_Mode::None;
    _selection_modes["Brush"] = Selection_Mode::Brush;

    ui->selection_mode_combo->addItems(QStringList(_selection_modes.keys()));

    connect(ui->selection_mode_combo, &QComboBox::currentTextChanged, this, &MediaMask_Widget::_toggleSelectionMode);

    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaMask_Widget::_setMaskAlpha);
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaMask_Widget::_setMaskColor);

    // Connect bounding box control
    connect(ui->show_bounding_box_checkbox, &QCheckBox::toggled,
            this, &MediaMask_Widget::_toggleShowBoundingBox);

    // Connect outline control
    connect(ui->show_outline_checkbox, &QCheckBox::toggled,
            this, &MediaMask_Widget::_toggleShowOutline);

    // Connect transparency control
    connect(ui->use_as_transparency_checkbox, &QCheckBox::toggled,
            this, &MediaMask_Widget::_toggleUseAsTransparency);

    // Setup the selection mode widgets
    _setupSelectionModePages();

    // Setup the dilation widget
    _setupDilationWidget();
}

MediaMask_Widget::~MediaMask_Widget() {
    delete ui;
}

void MediaMask_Widget::showEvent(QShowEvent * event) {

    static_cast<void>(event);

    std::cout << "MediaMask_Widget Show Event" << std::endl;
    connect(_scene, &Media_Window::leftClickCanvas, this, &MediaMask_Widget::_clickedInVideo);
    connect(_scene, &Media_Window::rightClickCanvas, this, &MediaMask_Widget::_rightClickedInVideo);
    connect(_scene, &Media_Window::mouseMoveCanvas, this, &MediaMask_Widget::_mouseMoveInVideo);
    connect(_scene, &Media_Window::leftRelease, this, &MediaMask_Widget::_mouseReleased);
    connect(_scene, &Media_Window::rightRelease, this, &MediaMask_Widget::_mouseReleased);
}

void MediaMask_Widget::hideEvent(QHideEvent * event) {

    static_cast<void>(event);

    std::cout << "MediaMask_Widget Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickCanvas, this, &MediaMask_Widget::_clickedInVideo);
    disconnect(_scene, &Media_Window::rightClickCanvas, this, &MediaMask_Widget::_rightClickedInVideo);
    disconnect(_scene, &Media_Window::mouseMoveCanvas, this, &MediaMask_Widget::_mouseMoveInVideo);
    disconnect(_scene, &Media_Window::leftRelease, this, &MediaMask_Widget::_mouseReleased);
    disconnect(_scene, &Media_Window::rightRelease, this, &MediaMask_Widget::_mouseReleased);

    // Clean up hover circle when switching away from mask widget
    _scene->setShowHoverCircle(false);
}

void MediaMask_Widget::_setupSelectionModePages() {
    // Create the "None" mode page using our widget
    _noneSelectionWidget = new mask_widget::MaskNoneSelectionWidget();
    ui->mode_stacked_widget->addWidget(_noneSelectionWidget);

    // Create the "Brush" mode page using our new widget
    _brushSelectionWidget = new mask_widget::MaskBrushSelectionWidget();
    ui->mode_stacked_widget->addWidget(_brushSelectionWidget);

    // Connect signals from the brush selection widget
    connect(_brushSelectionWidget, &mask_widget::MaskBrushSelectionWidget::brushSizeChanged,
            this, &MediaMask_Widget::_setBrushSize);
    connect(_brushSelectionWidget, &mask_widget::MaskBrushSelectionWidget::hoverCircleVisibilityChanged,
            this, &MediaMask_Widget::_toggleShowHoverCircle);
    connect(_brushSelectionWidget, &mask_widget::MaskBrushSelectionWidget::allowEmptyMaskChanged,
            this, &MediaMask_Widget::_onAllowEmptyMaskChanged);

    // Set initial page
    ui->mode_stacked_widget->setCurrentIndex(0);
}

void MediaMask_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color picker to the current mask color if available
    if (!key.empty()) {
        auto config = _scene->getMaskConfig(key);

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color()));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha() * 100));

            // Set bounding box checkbox
            ui->show_bounding_box_checkbox->blockSignals(true);
            ui->show_bounding_box_checkbox->setChecked(config.value()->show_bounding_box);
            ui->show_bounding_box_checkbox->blockSignals(false);

            // Set outline checkbox
            ui->show_outline_checkbox->blockSignals(true);
            ui->show_outline_checkbox->setChecked(config.value()->show_outline);
            ui->show_outline_checkbox->blockSignals(false);

            // Set transparency checkbox
            ui->use_as_transparency_checkbox->blockSignals(true);
            ui->use_as_transparency_checkbox->setChecked(config.value()->use_as_transparency);
            ui->use_as_transparency_checkbox->blockSignals(false);
        }
    }
}

void MediaMask_Widget::_setMaskAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->alpha() = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}

void MediaMask_Widget::_setMaskColor(QString const & hex_color) {
    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->hex_color() = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}

void MediaMask_Widget::_toggleSelectionMode(QString text) {
    _selection_mode = _selection_modes[text];

    // Switch to the appropriate page in the stacked widget
    int pageIndex = static_cast<int>(_selection_mode);
    ui->mode_stacked_widget->setCurrentIndex(pageIndex);

    // Update hover circle visibility based on the mode
    if (_selection_mode == Selection_Mode::Brush) {
        _scene->setShowHoverCircle(_brushSelectionWidget->isHoverCircleVisible());
        _scene->setHoverCircleRadius(static_cast<double>(_brushSelectionWidget->getBrushSize()));
    } else {
        _scene->setShowHoverCircle(false);
    }

    std::cout << "MediaMask_Widget selection mode changed to: " << text.toStdString() << std::endl;
}

void MediaMask_Widget::_clickedInVideo(CanvasCoordinates const & canvas_coords) {
    if (_active_key.empty()) {
        std::cout << "No active mask key" << std::endl;
        return;
    }

    std::cout << "Left clicked in video at canvas (" << canvas_coords.x << ", " << canvas_coords.y
              << ") with selection mode: " << static_cast<int>(_selection_mode) << std::endl;

    // Process the click based on selection mode
    switch (_selection_mode) {
        case Selection_Mode::None:
            // Do nothing in None mode
            break;
        case Selection_Mode::Brush:
            _is_dragging = true;
            _is_adding_mode = true;
            _addToMask(canvas_coords);
            break;
    }
}

void MediaMask_Widget::_rightClickedInVideo(CanvasCoordinates const & canvas_coords) {
    if (_active_key.empty() || _selection_mode != Selection_Mode::Brush) {
        return;
    }

    std::cout << "Right clicked in video at canvas (" << canvas_coords.x << ", " << canvas_coords.y << ")" << std::endl;

    _is_dragging = true;
    _is_adding_mode = false;
    _removeFromMask(canvas_coords);
}

void MediaMask_Widget::_setBrushSize(int size) {
    if (_selection_mode == Selection_Mode::Brush) {
        _scene->setHoverCircleRadius(static_cast<double>(size));
    }
    std::cout << "Brush size set to: " << size << std::endl;
}

void MediaMask_Widget::_toggleShowHoverCircle(bool checked) {
    if (_selection_mode == Selection_Mode::Brush) {
        _scene->setShowHoverCircle(checked);
    }
    std::cout << "Show hover circle " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaMask_Widget::_toggleShowBoundingBox(bool checked) {
    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->show_bounding_box = checked;
        }
        _scene->UpdateCanvas();
    }
    std::cout << "Show bounding box " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaMask_Widget::_toggleShowOutline(bool checked) {
    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->show_outline = checked;
        }
        _scene->UpdateCanvas();
    }
    std::cout << "Show outline " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaMask_Widget::_toggleUseAsTransparency(bool checked) {
    std::cout << "Transparency checkbox toggled: " << (checked ? "enabled" : "disabled") << std::endl;

    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->use_as_transparency = checked;
            std::cout << "Updated mask config for key: " << _active_key << std::endl;
        } else {
            std::cout << "No mask config found for key: " << _active_key << std::endl;
        }
        _scene->UpdateCanvas();
    } else {
        std::cout << "No active key set" << std::endl;
    }
    std::cout << "Use as transparency " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaMask_Widget::_setupDilationWidget() {
    // Create the dilation widget and section
    _dilation_widget = new MaskDilationWidget(this);
    _dilation_section = new Section(this, "Mask Dilation");
    _dilation_section->setContentLayout(*new QVBoxLayout());
    _dilation_section->layout()->addWidget(_dilation_widget);
    _dilation_section->autoSetContentLayout();

    // Connect dilation widget signals
    connect(_dilation_widget, &MaskDilationWidget::optionsChanged,
            this, &MediaMask_Widget::_onDilationOptionsChanged);
    connect(_dilation_widget, &MaskDilationWidget::applyRequested,
            this, &MediaMask_Widget::_onDilationApplyRequested);

    // Replace the placeholder widget with the dilation section
    auto * layout = qobject_cast<QVBoxLayout *>(ui->verticalLayout);
    if (layout) {
        // Find the placeholder widget
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem * item = layout->itemAt(i);
            if (item && item->widget() && item->widget() == ui->dilation_section_placeholder) {
                // Replace the placeholder with the dilation section
                layout->removeWidget(ui->dilation_section_placeholder);
                ui->dilation_section_placeholder->hide();
                layout->insertWidget(i, _dilation_section);
                break;
            }
        }
    }
}

void MediaMask_Widget::_onDilationOptionsChanged(MaskDilationOptions const & options) {
    if (options.active && options.preview) {
        _applyMaskDilation(options);
    } else {
        _restoreOriginalMaskData();
    }
}

void MediaMask_Widget::_onDilationApplyRequested() {
    _applyDilationPermanently();
}

void MediaMask_Widget::_applyMaskDilation(MaskDilationOptions const & options) {
    if (_active_key.empty()) {
        return;
    }

    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        return;
    }

    // Store original data if not already stored
    _storeOriginalMaskData();

    auto const & original_masks = _original_mask_data[_active_key];

    // Apply dilation to each mask at current time
    std::vector<Mask2D> dilated_masks;
    auto image_size = mask_data->getImageSize();

    for (auto const & single_mask: original_masks) {
        if (!single_mask.empty()) {
            auto dilated_mask = ImageProcessing::dilate_mask(single_mask.points(), image_size, options);
            dilated_masks.push_back(Mask2D(std::move(dilated_mask)));
        }
    }

    // Set preview data in the Media_Window
    _scene->setPreviewMaskData(_active_key, dilated_masks, true);
    _preview_active = true;

    // Update the display by updating the canvas
    _scene->UpdateCanvas();
}

void MediaMask_Widget::_applyDilationPermanently() {
    if (!_preview_active || _active_key.empty()) {
        return;
    }

    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        return;
    }

    auto preview_masks = _scene->getPreviewMaskData(_active_key);

    auto current_index_and_frame = _data_manager->getCurrentIndexAndFrame(TimeKey("time"));

    // Clear existing masks at this time
    mask_data->clearAtTime(current_index_and_frame,
                           NotifyObservers::No);

    // Add the dilated masks
    for (auto const & dilated_mask: preview_masks) {
        if (!dilated_mask.empty()) {
            mask_data->addAtTime(current_index_and_frame, dilated_mask, NotifyObservers::No);
        }
    }

    // Notify observers
    mask_data->notifyObservers();

    // Clear preview data
    _scene->setPreviewMaskData(_active_key, {}, false);
    _preview_active = false;
    _original_mask_data.clear();

    // Reset the dilation widget
    MaskDilationOptions default_options;
    _dilation_widget->setOptions(default_options);

    std::cout << "Mask dilation applied permanently" << std::endl;
}

void MediaMask_Widget::_storeOriginalMaskData() {
    if (_active_key.empty() || _original_mask_data.count(_active_key) > 0) {
        return;// Already stored or no active key
    }

    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        return;
    }

    // Get current time and store the original mask data
    auto current_time = _data_manager->getCurrentTime();
    auto const & masks_at_time = mask_data->getAtTime(TimeFrameIndex(current_time));

    _original_mask_data[_active_key] = std::vector<Mask2D>(masks_at_time.begin(), masks_at_time.end());
}

void MediaMask_Widget::_restoreOriginalMaskData() {
    if (!_preview_active) {
        return;
    }

    // Clear preview data from Media_Window
    _scene->setPreviewMaskData(_active_key, {}, false);
    _preview_active = false;

    // Update the display
    _scene->UpdateCanvas();
}

void MediaMask_Widget::_addToMask(CanvasCoordinates const & canvas_coords) {
    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        if (_debug_performance) {
            std::cout << "Error: Could not retrieve mask data for key: " << _active_key << std::endl;
        }
        return;
    }

    // Get mask image size for coordinate transformation
    auto mask_image_size = mask_data->getImageSize();
    if (mask_image_size.width <= 0 || mask_image_size.height <= 0) {
        if (_debug_performance) {
            std::cout << "Error: Invalid mask image size" << std::endl;
        }
        return;
    }

    // Get canvas size for transformation
    auto [canvas_width, canvas_height] = _scene->getCanvasSize();
    if (canvas_width <= 0 || canvas_height <= 0) {
        if (_debug_performance) {
            std::cout << "Error: Invalid canvas size" << std::endl;
        }
        return;
    }

    // Transform canvas coordinates directly to mask coordinates
    // This maintains separation of concerns - no need to access media data
    float x_mask_raw = (canvas_coords.x / static_cast<float>(canvas_width)) * static_cast<float>(mask_image_size.width);
    float y_mask_raw = (canvas_coords.y / static_cast<float>(canvas_height)) * static_cast<float>(mask_image_size.height);

    // Get brush size and scale it separately for each dimension to preserve circle shape
    int brush_radius_canvas = _brushSelectionWidget->getBrushSize();
    float scale_x = static_cast<float>(mask_image_size.width) / static_cast<float>(canvas_width);
    float scale_y = static_cast<float>(mask_image_size.height) / static_cast<float>(canvas_height);

    // Calculate scaled brush radius for each dimension
    float brush_radius_x = static_cast<float>(brush_radius_canvas) * scale_x;
    float brush_radius_y = static_cast<float>(brush_radius_canvas) * scale_y;

    // Use the transformed coordinates directly - hover circle is now centered on cursor
    float x_mask = x_mask_raw;
    float y_mask = y_mask_raw;

    // Generate ellipse pixels using the new general-purpose function
    auto brush_pixels = generate_ellipse_pixels(x_mask, y_mask, brush_radius_x, brush_radius_y);


    auto current_index_and_frame = _data_manager->getCurrentIndexAndFrame(TimeKey("time"));
    auto const & existing_masks = mask_data->getAtTime(current_index_and_frame);

    // Get or create the primary mask (index 0)
    std::vector<Point2D<uint32_t>> primary_mask;
    if (!existing_masks.empty()) {
        primary_mask = existing_masks[0].points();// Copy the existing mask at index 0
    }
    // If no masks exist, primary_mask starts empty

    // Create a set of existing pixels in the primary mask for fast lookup
    std::set<std::pair<int, int>> existing_pixel_set;
    for (auto const & point: primary_mask) {
        existing_pixel_set.insert({static_cast<int>(point.x),
                                   static_cast<int>(point.y)});
    }

    // Add new pixels that don't already exist in the primary mask
    int added_count = 0;
    for (auto const & pixel: brush_pixels) {
        std::pair<int, int> pixel_key = {static_cast<int>(pixel.x),
                                         static_cast<int>(pixel.y)};
        if (existing_pixel_set.find(pixel_key) == existing_pixel_set.end()) {
            primary_mask.push_back(pixel);
            existing_pixel_set.insert(pixel_key);// Update set to avoid adding duplicates within this operation
            added_count++;
        }
    }

    // Only update the mask data if we added new pixels
    if (added_count > 0) {
        // Clear all masks at this time
        mask_data->clearAtTime(current_index_and_frame,
                               NotifyObservers::No);
        mask_data->addAtTime(current_index_and_frame, Mask2D(std::move(primary_mask)), NotifyObservers::No);

        // Notify observers
        mask_data->notifyObservers();
    }

    if (_debug_performance) {
        std::cout << "BRUSH ADD: Added " << added_count << " new pixels (out of " << brush_pixels.size()
                  << " generated) to primary mask at index 0. Total mask size: "
                  << (primary_mask.size() - added_count + added_count) << " pixels" << std::endl;
    }
}

void MediaMask_Widget::_removeFromMask(CanvasCoordinates const & canvas_coords) {
    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        if (_debug_performance) {
            std::cout << "Error: Could not retrieve mask data for key: " << _active_key << std::endl;
        }
        return;
    }

    // Get mask image size for coordinate transformation
    auto mask_image_size = mask_data->getImageSize();
    if (mask_image_size.width <= 0 || mask_image_size.height <= 0) {
        if (_debug_performance) {
            std::cout << "Error: Invalid mask image size" << std::endl;
        }
        return;
    }

    // Get canvas size for transformation
    auto [canvas_width, canvas_height] = _scene->getCanvasSize();
    if (canvas_width <= 0 || canvas_height <= 0) {
        if (_debug_performance) {
            std::cout << "Error: Invalid canvas size" << std::endl;
        }
        return;
    }

    // Transform canvas coordinates directly to mask coordinates
    // This maintains separation of concerns - no need to access media data
    float x_mask_raw = (canvas_coords.x / static_cast<float>(canvas_width)) * static_cast<float>(mask_image_size.width);
    float y_mask_raw = (canvas_coords.y / static_cast<float>(canvas_height)) * static_cast<float>(mask_image_size.height);

    // Get brush size and scale it separately for each dimension to preserve circle shape
    int brush_radius_canvas = _brushSelectionWidget->getBrushSize();
    float scale_x = static_cast<float>(mask_image_size.width) / static_cast<float>(canvas_width);
    float scale_y = static_cast<float>(mask_image_size.height) / static_cast<float>(canvas_height);

    // Calculate scaled brush radius for each dimension
    float brush_radius_x = static_cast<float>(brush_radius_canvas) * scale_x;
    float brush_radius_y = static_cast<float>(brush_radius_canvas) * scale_y;

    // Use the transformed coordinates directly - hover circle is now centered on cursor
    float x_mask = x_mask_raw;
    float y_mask = y_mask_raw;

    // Get current time and existing masks
    auto current_index_and_frame = _data_manager->getCurrentIndexAndFrame(TimeKey("time"));
    auto const & existing_masks = mask_data->getAtTime(current_index_and_frame);

    // Check if there's a primary mask (index 0) to remove from
    if (existing_masks.empty()) {
        if (_debug_performance) {
            std::cout << "BRUSH REMOVE: No mask exists to remove from" << std::endl;
        }
        return;
    }

    // Work with the primary mask (index 0)
    auto const & primary_mask = existing_masks[0];
    std::vector<Point2D<uint32_t>> filtered_mask;
    int removed_count = 0;

    // Filter out pixels within brush radius from the primary mask
    for (auto const & point: primary_mask) {
        // Calculate elliptical distance from brush center
        float dx = point.x - x_mask;
        float dy = point.y - y_mask;
        float normalized_dx = dx / brush_radius_x;
        float normalized_dy = dy / brush_radius_y;
        float ellipse_distance = normalized_dx * normalized_dx + normalized_dy * normalized_dy;

        // Keep pixels outside brush ellipse
        if (ellipse_distance > 1.0f) {
            filtered_mask.push_back(point);
        } else {
            removed_count++;
        }
    }

    // Only update if we actually removed pixels
    if (removed_count > 0) {

        mask_data->clearAtTime(current_index_and_frame,
                               NotifyObservers::No);

        // Add the filtered mask back if it still has points OR if empty masks are allowed
        if (!filtered_mask.empty() || _allow_empty_mask) {
            mask_data->addAtTime(current_index_and_frame, Mask2D(std::move(filtered_mask)), NotifyObservers::No);
        }

        // Notify observers
        mask_data->notifyObservers();
    }

    if (_debug_performance) {
        std::cout << "BRUSH REMOVE: Removed " << removed_count << " pixels from primary mask at index 0. "
                  << "Remaining mask size: " << filtered_mask.size() << " pixels" << std::endl;
    }
}

void MediaMask_Widget::_mouseMoveInVideo(CanvasCoordinates const & canvas_coords) {
    // Only process mouse move if we're in brush mode and currently dragging
    if (_active_key.empty() || _selection_mode != Selection_Mode::Brush || !_is_dragging) {
        return;
    }

    // Continue adding or removing based on the mode set when dragging started
    if (_is_adding_mode) {
        _addToMask(canvas_coords);
    } else {
        _removeFromMask(canvas_coords);
    }
}

void MediaMask_Widget::_mouseReleased() {
    // Stop dragging when mouse is released
    bool was_dragging = _is_dragging;
    _is_dragging = false;

    // Update canvas once when brush drag operation is completed
    if (_selection_mode == Selection_Mode::Brush && was_dragging) {
        _scene->UpdateCanvas();
        if (_debug_performance) {
            std::cout << "Brush drag operation completed, canvas updated" << std::endl;
        }
    }
}

void MediaMask_Widget::_onAllowEmptyMaskChanged(bool allow) {
    _allow_empty_mask = allow;
    if (_debug_performance) {
        std::cout << "Allow empty mask setting changed to: " << (allow ? "enabled" : "disabled") << std::endl;
    }
}

void MediaMask_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);

    // Ensure the color picker and stacked widget components resize properly
    int availableWidth = width() - 20;// Account for margins

    // Update color picker width to use available space
    ui->color_picker->setFixedWidth(qMin(availableWidth, 400));// Cap at reasonable maximum

    // Update stacked widget width
    ui->mode_stacked_widget->setFixedWidth(availableWidth);

    // Update all widgets in the stacked widget
    for (int i = 0; i < ui->mode_stacked_widget->count(); ++i) {
        QWidget * widget = ui->mode_stacked_widget->widget(i);
        if (widget) {
            widget->setFixedWidth(availableWidth);
        }
    }
}