#include "MediaMask_Widget.hpp"
#include "ui_MediaMask_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "Media_Window/Media_Window.hpp"
#include "SelectionWidgets/MaskNoneSelectionWidget.hpp"
#include "SelectionWidgets/MaskDrawSelectionWidget.hpp"

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
    _selection_modes["Draw"] = Selection_Mode::Draw;
    _selection_modes["Erase"] = Selection_Mode::Erase;

    ui->selection_mode_combo->addItems(QStringList(_selection_modes.keys()));
    
    connect(ui->selection_mode_combo, &QComboBox::currentTextChanged, this, &MediaMask_Widget::_toggleSelectionMode);

    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaMask_Widget::_setMaskAlpha);
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaMask_Widget::_setMaskColor);
            
    // Setup the selection mode widgets
    _setupSelectionModePages();
}

MediaMask_Widget::~MediaMask_Widget() {
    delete ui;
}

void MediaMask_Widget::showEvent(QShowEvent * event) {
    std::cout << "MediaMask_Widget Show Event" << std::endl;
    connect(_scene, &Media_Window::leftClickMedia, this, &MediaMask_Widget::_clickedInVideo);
}

void MediaMask_Widget::hideEvent(QHideEvent * event) {
    std::cout << "MediaMask_Widget Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickMedia, this, &MediaMask_Widget::_clickedInVideo);
}

void MediaMask_Widget::_setupSelectionModePages() {
    // Create the "None" mode page using our widget
    _noneSelectionWidget = new mask_widget::MaskNoneSelectionWidget();
    ui->mode_stacked_widget->addWidget(_noneSelectionWidget);
    
    // Create the "Draw" mode page using our new widget
    _drawSelectionWidget = new mask_widget::MaskDrawSelectionWidget();
    ui->mode_stacked_widget->addWidget(_drawSelectionWidget);
    
    // Connect signals from the draw selection widget
    connect(_drawSelectionWidget, &mask_widget::MaskDrawSelectionWidget::brushSizeChanged,
            this, &MediaMask_Widget::_setBrushSize);
    connect(_drawSelectionWidget, &mask_widget::MaskDrawSelectionWidget::hoverCircleVisibilityChanged,
            this, &MediaMask_Widget::_toggleShowHoverCircle);
    
    // Create placeholder widget for Erase mode
    // This will be replaced with a proper widget in future implementations
    QWidget* erasePage = new QWidget();
    QVBoxLayout* eraseLayout = new QVBoxLayout(erasePage);
    QLabel* eraseLabel = new QLabel("Erase mode: Click and drag in the video to erase parts of the mask.");
    eraseLabel->setWordWrap(true);
    eraseLayout->addWidget(eraseLabel);
    ui->mode_stacked_widget->addWidget(erasePage);
    
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
    if (_selection_mode == Selection_Mode::Draw) {
        _scene->setShowHoverCircle(_drawSelectionWidget->isHoverCircleVisible());
        _scene->setHoverCircleRadius(static_cast<double>(_drawSelectionWidget->getBrushSize()));
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

    std::cout << "Clicked in video at (" << x_canvas << ", " << y_canvas 
              << ") with selection mode: " << static_cast<int>(_selection_mode) << std::endl;
              
    // Process the click based on selection mode
    switch (_selection_mode) {
        case Selection_Mode::None:
            // Do nothing in None mode
            break;
        case Selection_Mode::Draw:
            // In future implementation: handle drawing
            std::cout << "Draw mode clicked (not yet implemented)" << std::endl;
            break;
        case Selection_Mode::Erase:
            // In future implementation: handle erasing
            std::cout << "Erase mode clicked (not yet implemented)" << std::endl;
            break;
    }
}

void MediaMask_Widget::_setBrushSize(int size) {
    if (_selection_mode == Selection_Mode::Draw) {
        _scene->setHoverCircleRadius(static_cast<double>(size));
    }
    std::cout << "Brush size set to: " << size << std::endl;
}

void MediaMask_Widget::_toggleShowHoverCircle(bool checked) {
    if (_selection_mode == Selection_Mode::Draw) {
        _scene->setShowHoverCircle(checked);
    }
    std::cout << "Show hover circle " << (checked ? "enabled" : "disabled") << std::endl;
}
