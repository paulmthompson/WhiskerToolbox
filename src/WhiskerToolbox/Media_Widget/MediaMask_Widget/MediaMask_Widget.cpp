#include "MediaMask_Widget.hpp"
#include "ui_MediaMask_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/utils/opencv_utility.hpp"
#include "Media_Window/Media_Window.hpp"
#include "SelectionWidgets/MaskNoneSelectionWidget.hpp"
#include "SelectionWidgets/MaskBrushSelectionWidget.hpp"
#include "MaskDilationWidget/MaskDilationWidget.hpp"
#include "../../Collapsible_Widget/Section.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <iostream>

MediaMask_Widget::MediaMask_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaMask_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene} {
    ui->setupUi(this);

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
            
    // Setup the selection mode widgets
    _setupSelectionModePages();
    
    // Setup the dilation widget
    _setupDilationWidget();
}

MediaMask_Widget::~MediaMask_Widget() {
    delete ui;
}

void MediaMask_Widget::showEvent(QShowEvent * event) {
    std::cout << "MediaMask_Widget Show Event" << std::endl;
    connect(_scene, &Media_Window::leftClickMedia, this, &MediaMask_Widget::_clickedInVideo);
    connect(_scene, &Media_Window::rightClickMedia, this, &MediaMask_Widget::_rightClickedInVideo);
}

void MediaMask_Widget::hideEvent(QHideEvent * event) {
    std::cout << "MediaMask_Widget Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickMedia, this, &MediaMask_Widget::_clickedInVideo);
    disconnect(_scene, &Media_Window::rightClickMedia, this, &MediaMask_Widget::_rightClickedInVideo);
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
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));
            
            // Set bounding box checkbox
            ui->show_bounding_box_checkbox->blockSignals(true);
            ui->show_bounding_box_checkbox->setChecked(config.value()->show_bounding_box);
            ui->show_bounding_box_checkbox->blockSignals(false);
            
            // Set outline checkbox
            ui->show_outline_checkbox->blockSignals(true);
            ui->show_outline_checkbox->setChecked(config.value()->show_outline);
            ui->show_outline_checkbox->blockSignals(false);
        }
    }
}

void MediaMask_Widget::_setMaskAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->alpha = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}

void MediaMask_Widget::_setMaskColor(const QString& hex_color) {
    if (!_active_key.empty()) {
        auto mask_opts = _scene->getMaskConfig(_active_key);
        if (mask_opts.has_value()) {
            mask_opts.value()->hex_color = hex_color.toStdString();
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

void MediaMask_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {
    if (_active_key.empty()) {
        std::cout << "No active mask key" << std::endl;
        return;
    }

    std::cout << "Left clicked in video at (" << x_canvas << ", " << y_canvas 
              << ") with selection mode: " << static_cast<int>(_selection_mode) << std::endl;
              
    // Process the click based on selection mode
    switch (_selection_mode) {
        case Selection_Mode::None:
            // Do nothing in None mode
            break;
        case Selection_Mode::Brush:
            // Add to mask (would be implemented in the future)
            std::cout << "Brush mode: Add to mask at (" << x_canvas << ", " << y_canvas 
                      << ") with radius " << _brushSelectionWidget->getBrushSize() << std::endl;
            break;
    }
}

void MediaMask_Widget::_rightClickedInVideo(qreal x_canvas, qreal y_canvas) {
    if (_active_key.empty() || _selection_mode != Selection_Mode::Brush) {
        return;
    }
    
    std::cout << "Right clicked in video at (" << x_canvas << ", " << y_canvas << ")" << std::endl;
    
    // Erase from mask (would be implemented in the future)
    std::cout << "Brush mode: Erase from mask at (" << x_canvas << ", " << y_canvas 
              << ") with radius " << _brushSelectionWidget->getBrushSize() << std::endl;
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
    auto* layout = qobject_cast<QVBoxLayout*>(ui->verticalLayout);
    if (layout) {
        // Find the placeholder widget
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem* item = layout->itemAt(i);
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

void MediaMask_Widget::_onDilationOptionsChanged(MaskDilationOptions const& options) {
    if (options.active && options.preview) {
        _applyMaskDilation(options);
    } else {
        _restoreOriginalMaskData();
    }
}

void MediaMask_Widget::_onDilationApplyRequested() {
    _applyDilationPermanently();
}

void MediaMask_Widget::_applyMaskDilation(MaskDilationOptions const& options) {
    if (_active_key.empty()) {
        return;
    }
    
    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        return;
    }
    
    // Store original data if not already stored
    _storeOriginalMaskData();
    
    // Get current time
    auto current_time = _data_manager->getCurrentTime();
    auto const& original_masks = _original_mask_data[_active_key];
    
    // Apply dilation to each mask at current time
    std::vector<std::vector<Point2D<float>>> dilated_masks;
    auto image_size = mask_data->getImageSize();
    
    for (auto const& single_mask : original_masks) {
        if (!single_mask.empty()) {
            auto dilated_mask = dilate_mask(single_mask, image_size, options);
            dilated_masks.push_back(dilated_mask);
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
    
    // Get current time and get the preview data from Media_Window
    auto current_time = _data_manager->getCurrentTime();
    auto preview_masks = _scene->getPreviewMaskData(_active_key, current_time);
    
    // Clear existing masks at this time
    mask_data->clearAtTime(current_time, false);
    
    // Add the dilated masks
    for (auto const& dilated_mask : preview_masks) {
        if (!dilated_mask.empty()) {
            mask_data->addAtTime(current_time, dilated_mask, false);
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
        return; // Already stored or no active key
    }
    
    auto mask_data = _data_manager->getData<MaskData>(_active_key);
    if (!mask_data) {
        return;
    }
    
    // Get current time and store the original mask data
    auto current_time = _data_manager->getCurrentTime();
    auto const& masks_at_time = mask_data->getAtTime(current_time);
    
    _original_mask_data[_active_key] = masks_at_time;
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
