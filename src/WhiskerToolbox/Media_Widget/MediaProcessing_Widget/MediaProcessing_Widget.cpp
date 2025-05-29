#include "MediaProcessing_Widget.hpp"
#include "ui_MediaProcessing_Widget.h"
#include "ProcessingOptions/ContrastWidget.hpp"
#include "ProcessingOptions/GammaWidget.hpp"

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
      _gamma_section(nullptr) {
    
    ui->setupUi(this);
    _setupProcessingWidgets();
    
    // Connect legacy controls for other filters (these will be refactored later)
    // ... (sharpen, CLAHE, bilateral controls would go here)
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
    _contrast_section = new Section(this, "Linear Transform (Contrast/Brightness)");
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

// Legacy implementations for other filters (to be refactored later)
void MediaProcessing_Widget::_updateSharpenFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_sharpen_active) {
        // TODO: Create SharpenOptions and use options-based function
        media_data->setProcess("3__sharpentransform", [this](cv::Mat& input) {
            sharpen_image(input, _sharpen_sigma); // Legacy function call
        });
    } else {
        media_data->removeProcess("3__sharpentransform");
    }
    
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_updateClaheFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_clahe_active) {
        // TODO: Create ClaheOptions and use options-based function
        media_data->setProcess("4__clahe", [this](cv::Mat& input) {
            clahe(input, _clahe_clip, _clahe_grid); // Legacy function call
        });
    } else {
        media_data->removeProcess("4__clahe");
    }
    
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

void MediaProcessing_Widget::_updateBilateralFilter() {
    if (_active_key.empty()) return;
    
    auto media_data = _data_manager->getData<MediaData>(_active_key);
    if (!media_data) return;
    
    if (_bilateral_active) {
        // TODO: Create BilateralOptions and use options-based function
        media_data->setProcess("5__bilateral", [this](cv::Mat& input) {
            bilateral_filter(input, _bilateral_d, _bilateral_color_sigma, _bilateral_spatial_sigma); // Legacy function call
        });
    } else {
        media_data->removeProcess("5__bilateral");
    }
    
    if (_scene) {
        _scene->UpdateCanvas();
    }
}

// Legacy slot implementations (to be removed when other filters are refactored)
void MediaProcessing_Widget::_updateSharpenSigma() {
    // Implementation for sharpen sigma update
}

void MediaProcessing_Widget::_activateSharpen() {
    // Implementation for sharpen activation
}

void MediaProcessing_Widget::_updateClaheGrid() {
    // Implementation for CLAHE grid update
}

void MediaProcessing_Widget::_updateClaheClip() {
    // Implementation for CLAHE clip update
}

void MediaProcessing_Widget::_activateClahe() {
    // Implementation for CLAHE activation
}

void MediaProcessing_Widget::_updateBilateralD() {
    // Implementation for bilateral D update
}

void MediaProcessing_Widget::_updateBilateralSpatialSigma() {
    // Implementation for bilateral spatial sigma update
}

void MediaProcessing_Widget::_updateBilateralColorSigma() {
    // Implementation for bilateral color sigma update
}

void MediaProcessing_Widget::_activateBilateral() {
    // Implementation for bilateral activation
} 
