#include "MediaProcessing_Widget.hpp"
#include "ui_MediaProcessing_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "Media_Window/Media_Window.hpp"
#include "DataManager/utils/opencv_utility.hpp"

#include <QCheckBox>
#include <iostream>

MediaProcessing_Widget::MediaProcessing_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MediaProcessing_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene} {
    
    ui->setupUi(this);

    // Connect contrast controls
    connect(ui->alpha_dspinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateContrastAlpha);
    connect(ui->beta_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateContrastBeta);
    connect(ui->contrast_checkbox, &QCheckBox::checkStateChanged, 
            this, &MediaProcessing_Widget::_activateContrast);

    // Connect gamma controls
    connect(ui->gamma_dspinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateGamma);
    connect(ui->gamma_checkbox, &QCheckBox::checkStateChanged, 
            this, &MediaProcessing_Widget::_activateGamma);

    // Connect sharpen controls
    connect(ui->sharpen_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateSharpenSigma);
    connect(ui->sharpen_checkbox, &QCheckBox::checkStateChanged, 
            this, &MediaProcessing_Widget::_activateSharpen);

    // Connect CLAHE controls
    connect(ui->clahe_grid_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateClaheGrid);
    connect(ui->clahe_clip_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateClaheClip);
    connect(ui->clahe_checkbox, &QCheckBox::checkStateChanged, 
            this, &MediaProcessing_Widget::_activateClahe);

    // Connect bilateral filter controls
    connect(ui->bilateral_d_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateBilateralD);
    connect(ui->bilateral_spatial_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateBilateralSpatialSigma);
    connect(ui->bilateral_color_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &MediaProcessing_Widget::_updateBilateralColorSigma);
    connect(ui->bilateral_checkbox, &QCheckBox::checkStateChanged, 
            this, &MediaProcessing_Widget::_activateBilateral);
}

MediaProcessing_Widget::~MediaProcessing_Widget() {
    delete ui;
}

void MediaProcessing_Widget::setActiveKey(std::string const& key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    // Initialize UI controls from current settings
    // The controls will retain their current values, which is appropriate
    // since the MediaData process chain is global to the media data
}

//////////////////////////////////////////////////
// Contrast/Linear Transform
//////////////////////////////////////////////////

void MediaProcessing_Widget::_updateContrastFilter() {
    if (_contrast_active && !_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->setProcess("1__lineartransform", [this](cv::Mat & input) {
                linear_transform(input, _contrast_alpha, _contrast_beta);
            });
        }
    }
}

void MediaProcessing_Widget::_activateContrast() {
    _contrast_active = ui->contrast_checkbox->isChecked();

    if (_contrast_active) {
        _updateContrastFilter();
    } else if (!_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->removeProcess("1__lineartransform");
        }
    }
}

void MediaProcessing_Widget::_updateContrastAlpha() {
    _contrast_alpha = ui->alpha_dspinbox->value();
    _updateContrastFilter();

    if (!_contrast_active) {
        ui->contrast_checkbox->setChecked(true);
    }
}

void MediaProcessing_Widget::_updateContrastBeta() {
    _contrast_beta = ui->beta_spinbox->value();
    _updateContrastFilter();

    if (!_contrast_active) {
        ui->contrast_checkbox->setChecked(true);
    }
}

//////////////////////////////////////////////////
// Gamma
//////////////////////////////////////////////////

void MediaProcessing_Widget::_updateGammaFilter() {
    if (_gamma_active && !_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->setProcess("1__gamma", [this](cv::Mat & input) {
                gamma_transform(input, _gamma);
            });
        }
    }
}

void MediaProcessing_Widget::_activateGamma() {
    _gamma_active = ui->gamma_checkbox->isChecked();

    if (_gamma_active) {
        _updateGammaFilter();
    } else if (!_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->removeProcess("1__gamma");
        }
    }
}

void MediaProcessing_Widget::_updateGamma() {
    _gamma = ui->gamma_dspinbox->value();
    _updateGammaFilter();

    if (!_gamma_active) {
        ui->gamma_checkbox->setChecked(true);
    }
}

//////////////////////////////////////////////////
// Sharpen
//////////////////////////////////////////////////

void MediaProcessing_Widget::_updateSharpenFilter() {
    if (_sharpen_active && !_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->setProcess("2__sharpentransform", [this](cv::Mat & input) {
                sharpen_image(input, _sharpen_sigma);
            });
        }
    }
}

void MediaProcessing_Widget::_activateSharpen() {
    _sharpen_active = ui->sharpen_checkbox->isChecked();

    if (_sharpen_active) {
        _updateSharpenFilter();
    } else if (!_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->removeProcess("2__sharpentransform");
        }
    }
}

void MediaProcessing_Widget::_updateSharpenSigma() {
    _sharpen_sigma = ui->sharpen_spinbox->value();
    _updateSharpenFilter();

    if (!_sharpen_active) {
        ui->sharpen_checkbox->setChecked(true);
    }
}

//////////////////////////////////////////////////
// CLAHE
//////////////////////////////////////////////////

void MediaProcessing_Widget::_updateClaheFilter() {
    if (_clahe_active && !_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->setProcess("3__clahetransform", [this](cv::Mat & input) {
                clahe(input, _clahe_clip, _clahe_grid);
            });
        }
    }
}

void MediaProcessing_Widget::_activateClahe() {
    _clahe_active = ui->clahe_checkbox->isChecked();

    if (_clahe_active) {
        _updateClaheFilter();
    } else if (!_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->removeProcess("3__clahetransform");
        }
    }
}

void MediaProcessing_Widget::_updateClaheClip() {
    _clahe_clip = ui->clahe_clip_spinbox->value();
    _updateClaheFilter();

    if (!_clahe_active) {
        ui->clahe_checkbox->setChecked(true);
    }
}

void MediaProcessing_Widget::_updateClaheGrid() {
    _clahe_grid = ui->clahe_grid_spinbox->value();
    _updateClaheFilter();

    if (!_clahe_active) {
        ui->clahe_checkbox->setChecked(true);
    }
}

//////////////////////////////////////////////////
// Bilateral Filter
//////////////////////////////////////////////////

void MediaProcessing_Widget::_updateBilateralFilter() {
    if (_bilateral_active && !_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->setProcess("4__bilateraltransform", [this](cv::Mat & input) {
                bilateral_filter(input, _bilateral_d, _bilateral_color_sigma, _bilateral_spatial_sigma);
            });
        }
    }
}

void MediaProcessing_Widget::_activateBilateral() {
    _bilateral_active = ui->bilateral_checkbox->isChecked();

    if (_bilateral_active) {
        _updateBilateralFilter();
    } else if (!_active_key.empty()) {
        auto media = _data_manager->getData<MediaData>(_active_key);
        if (media) {
            media->removeProcess("4__bilateraltransform");
        }
    }
}

void MediaProcessing_Widget::_updateBilateralD() {
    _bilateral_d = ui->bilateral_d_spinbox->value();
    _updateBilateralFilter();

    if (!_bilateral_active) {
        ui->bilateral_checkbox->setChecked(true);
    }
}

void MediaProcessing_Widget::_updateBilateralSpatialSigma() {
    _bilateral_spatial_sigma = ui->bilateral_spatial_spinbox->value();
    _updateBilateralFilter();

    if (!_bilateral_active) {
        ui->bilateral_checkbox->setChecked(true);
    }
}

void MediaProcessing_Widget::_updateBilateralColorSigma() {
    _bilateral_color_sigma = ui->bilateral_color_spinbox->value();
    _updateBilateralFilter();

    if (!_bilateral_active) {
        ui->bilateral_checkbox->setChecked(true);
    }
} 