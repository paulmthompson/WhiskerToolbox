#include "MediaProcessing_Widget.hpp"
#include "ui_MediaProcessing_Widget.h"
#include "ProcessingOptions/ContrastWidget.hpp"
#include "ProcessingOptions/GammaWidget.hpp"
#include "ProcessingOptions/SharpenWidget.hpp"
#include "ProcessingOptions/ClaheWidget.hpp"
#include "ProcessingOptions/BilateralWidget.hpp"
#include "ProcessingOptions/MedianWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/utils/opencv_utility.hpp"
#include "Media_Window/Media_Window.hpp"
#include "Collapsible_Widget/Section.hpp"

#include <QVBoxLayout>
#include <QScrollArea>
#include <iostream>

MediaProcessing_Widget::MediaProcessing_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MediaProcessing_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _contrast_widget(nullptr),
      _contrast_section(nullptr),
      _gamma_widget(nullptr),
      _gamma_section(nullptr),
      _sharpen_widget(nullptr),
      _sharpen_section(nullptr),
      _clahe_widget(nullptr),
      _clahe_section(nullptr),
      _bilateral_widget(nullptr),
      _bilateral_section(nullptr),
      _median_widget(nullptr),
      _median_section(nullptr) {
    
    ui->setupUi(this);
    _setupProcessingWidgets();
    
    // Connect legacy controls for other filters (these will be refactored later)
    // ... (CLAHE controls would go here)
}

MediaProcessing_Widget::~MediaProcessing_Widget() {
    delete ui;
}

void MediaProcessing_Widget::setActiveKey(std::string const& key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    std::cout << "MediaProcessing_Widget active key set to: " << key << std::endl;
}

void MediaProcessing_Widget::_setupProcessingWidgets() {
    // Get the scroll area's content layout
    auto* scroll_layout = ui->scroll_layout;
    
    // Create contrast section
    _contrast_widget = new ContrastWidget(this);
    _contrast_section = new Section(this, "Linear Transform");
    _contrast_section->setContentLayout(*new QVBoxLayout());
    _contrast_section->layout()->addWidget(_contrast_widget);
    _contrast_section->autoSetContentLayout();
    
    // Connect contrast widget signals
    connect(_contrast_widget, &ContrastWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onContrastOptionsChanged);
    
    // Add contrast section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _contrast_section);
    
    // Set initial contrast options
    _contrast_widget->setOptions(_contrast_options);
    
    // Create gamma section
    _gamma_widget = new GammaWidget(this);
    _gamma_section = new Section(this, "Gamma Correction");
    _gamma_section->setContentLayout(*new QVBoxLayout());
    _gamma_section->layout()->addWidget(_gamma_widget);
    _gamma_section->autoSetContentLayout();
    
    // Connect gamma widget signals
    connect(_gamma_widget, &GammaWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onGammaOptionsChanged);
    
    // Add gamma section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _gamma_section);
    
    // Set initial gamma options
    _gamma_widget->setOptions(_gamma_options);
    
    // Create sharpen section
    _sharpen_widget = new SharpenWidget(this);
    _sharpen_section = new Section(this, "Image Sharpening");
    _sharpen_section->setContentLayout(*new QVBoxLayout());
    _sharpen_section->layout()->addWidget(_sharpen_widget);
    _sharpen_section->autoSetContentLayout();
    
    // Connect sharpen widget signals
    connect(_sharpen_widget, &SharpenWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onSharpenOptionsChanged);
    
    // Add sharpen section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _sharpen_section);
    
    // Set initial sharpen options
    _sharpen_widget->setOptions(_sharpen_options);
    
    // Create clahe section
    _clahe_widget = new ClaheWidget(this);
    _clahe_section = new Section(this, "CLAHE");
    _clahe_section->setContentLayout(*new QVBoxLayout());
    _clahe_section->layout()->addWidget(_clahe_widget);
    _clahe_section->autoSetContentLayout();
    
    // Connect clahe widget signals
    connect(_clahe_widget, &ClaheWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onClaheOptionsChanged);
    
    // Add clahe section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _clahe_section);
    
    // Set initial clahe options
    _clahe_widget->setOptions(_clahe_options);
    
    // Create bilateral section
    _bilateral_widget = new BilateralWidget(this);
    _bilateral_section = new Section(this, "Bilateral Filter");
    _bilateral_section->setContentLayout(*new QVBoxLayout());
    _bilateral_section->layout()->addWidget(_bilateral_widget);
    _bilateral_section->autoSetContentLayout();
    
    // Connect bilateral widget signals
    connect(_bilateral_widget, &BilateralWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onBilateralOptionsChanged);
    
    // Add bilateral section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _bilateral_section);
    
    // Set initial bilateral options
    _bilateral_widget->setOptions(_bilateral_options);
    
    // Create median section
    _median_widget = new MedianWidget(this);
    _median_section = new Section(this, "Median Filter");
    _median_section->setContentLayout(*new QVBoxLayout());
    _median_section->layout()->addWidget(_median_widget);
    _median_section->autoSetContentLayout();
    
    // Connect median widget signals
    connect(_median_widget, &MedianWidget::optionsChanged,
            this, &MediaProcessing_Widget::_onMedianOptionsChanged);
    
    // Add median section to the scroll layout (before the spacer)
    scroll_layout->insertWidget(scroll_layout->count() - 1, _median_section);
    
    // Set initial median options
    _median_widget->setOptions(_median_options);
}

void MediaProcessing_Widget::_onContrastOptionsChanged(ContrastOptions const& options) {
    _contrast_options = options;
    _applyContrastFilter();
    
    std::cout << "Contrast options changed - Active: " << options.active 
              << ", Alpha: " << options.alpha << ", Beta: " << options.beta << std::endl;
}

void MediaProcessing_Widget::_onGammaOptionsChanged(GammaOptions const& options) {
    _gamma_options = options;
    _applyGammaFilter();
    
    std::cout << "Gamma options changed - Active: " << options.active 
              << ", Gamma: " << options.gamma << std::endl;
}

void MediaProcessing_Widget::_onSharpenOptionsChanged(SharpenOptions const& options) {
    _sharpen_options = options;
    _applySharpenFilter();
    
    std::cout << "Sharpen options changed - Active: " << options.active 
              << ", Sigma: " << options.sigma << std::endl;
}

void MediaProcessing_Widget::_onClaheOptionsChanged(ClaheOptions const& options) {
    _clahe_options = options;
    _applyClaheFilter();
    
    std::cout << "CLAHE options changed - Active: " << options.active 
              << ", Clip Limit: " << options.clip_limit << ", Grid Size: " << options.grid_size << std::endl;
}

void MediaProcessing_Widget::_onBilateralOptionsChanged(BilateralOptions const& options) {
    _bilateral_options = options;
    _applyBilateralFilter();
    
    std::cout << "Bilateral options changed - Active: " << options.active
              << ", D: " << options.diameter << ", Color Sigma: " << options.sigma_color
              << ", Space Sigma: " << options.sigma_spatial << std::endl;
}

void MediaProcessing_Widget::_onMedianOptionsChanged(MedianOptions const& options) {
    _median_options = options;
    _applyMedianFilter();
    
    std::cout << "Median options changed - Active: " << options.active
              << ", Kernel Size: " << options.kernel_size << std::endl;
}

void MediaProcessing_Widget::_applyContrastFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_contrast_options.active) {
        // Add or update the contrast filter in the processing chain using the options structure
        media_data->setProcess("1__lineartransform", [options = _contrast_options](cv::Mat& input) {
            linear_transform(input, options);
        });
    } else {
        // Remove the contrast filter from the processing chain
        media_data->removeProcess("1__lineartransform");
    }
    
    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyGammaFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_gamma_options.active) {
        // Add or update the gamma filter in the processing chain using the options structure
        media_data->setProcess("2__gamma", [options = _gamma_options](cv::Mat& input) {
            gamma_transform(input, options);
        });
    } else {
        // Remove the gamma filter from the processing chain
        media_data->removeProcess("2__gamma");
    }
    
    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applySharpenFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_sharpen_options.active) {
        // Add or update the sharpen filter in the processing chain using the options structure
        media_data->setProcess("3__sharpen", [options = _sharpen_options](cv::Mat& input) {
            sharpen_image(input, options);
        });
    } else {
        // Remove the sharpen filter from the processing chain
        media_data->removeProcess("3__sharpen");
    }
    
    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyClaheFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_clahe_options.active) {
        // Add or update the CLAHE filter in the processing chain using the options structure
        media_data->setProcess("4__clahe", [options = _clahe_options](cv::Mat& input) {
            clahe(input, options);
        });
    } else {
        // Remove the CLAHE filter from the processing chain
        media_data->removeProcess("4__clahe");
    }
    
    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyBilateralFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_bilateral_options.active) {
        // Add or update the bilateral filter in the processing chain using the options structure
        media_data->setProcess("5__bilateral", [options = _bilateral_options](cv::Mat& input) {
            bilateral_filter(input, options);
        });
    } else {
        // Remove the bilateral filter from the processing chain
        media_data->removeProcess("5__bilateral");
    }
    
    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_applyMedianFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_median_options.active) {
        // Add or update the median filter in the processing chain using the options structure
        media_data->setProcess("6__median", [options = _median_options](cv::Mat& input) {
            median_filter(input, options);
        });
    } else {
        // Remove the median filter from the processing chain
        media_data->removeProcess("6__median");
    }
    
    // Update the canvas
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

