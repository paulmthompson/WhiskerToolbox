
#include "Image_Processing_Widget.hpp"
#include "ui_Image_Processing_Widget.h"

#include "DataManager/DataManager.hpp"
#include "Media_Window.hpp"
#include "utils/opencv_utility.hpp"

#include <QCheckBox>

#include <functional>
#include <iostream>


Image_Processing_Widget::Image_Processing_Widget(Media_Window* scene, std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Image_Processing_Widget),
    _data_manager{data_manager},
    _scene{scene}
{
    ui->setupUi(this);

    connect(ui->alpha_dspinbox, &QDoubleSpinBox::valueChanged, this, &Image_Processing_Widget::_updateContrastAlpha);
    connect(ui->beta_spinbox, &QSpinBox::valueChanged, this, &Image_Processing_Widget::_updateContrastBeta);
    connect(ui->contrast_checkbox, &QCheckBox::checkStateChanged, this, &Image_Processing_Widget::_activateContrast);

    connect(ui->gamma_dspinbox, &QDoubleSpinBox::valueChanged, this, &Image_Processing_Widget::_updateGamma);
    connect(ui->gamma_checkbox, &QCheckBox::checkStateChanged, this, &Image_Processing_Widget::_activateGamma);

    connect(ui->sharpen_spinbox, &QDoubleSpinBox::valueChanged, this, &Image_Processing_Widget::_updateSharpenSigma);
    connect(ui->sharpen_checkbox, &QCheckBox::checkStateChanged, this, &Image_Processing_Widget::_activateSharpen);

    connect(ui->clahe_grid_spinbox, &QSpinBox::valueChanged, this, &Image_Processing_Widget::_updateClaheGrid);
    connect(ui->clahe_clip_spinbox, &QDoubleSpinBox::valueChanged, this, &Image_Processing_Widget::_updateClaheClip);
    connect(ui->clahe_checkbox, &QCheckBox::checkStateChanged, this, &Image_Processing_Widget::_activateClahe);

    connect(ui->bilateral_d_spinbox, &QSpinBox::valueChanged, this, &Image_Processing_Widget::_updateBilateralD);
    connect(ui->bilateral_spatial_spinbox, &QDoubleSpinBox::valueChanged, this, &Image_Processing_Widget::_updateBilateralSpatialSigma);
    connect(ui->bilateral_color_spinbox, &QDoubleSpinBox::valueChanged, this, &Image_Processing_Widget::_updateBilateralColorSigma);
    connect(ui->bilateral_checkbox, &QCheckBox::checkStateChanged, this, &Image_Processing_Widget::_activateBilateral);

    _data_manager->addCallbackToData("media", [this]() {
        _scene->UpdateCanvas();
    });
}

void Image_Processing_Widget::openWidget() {

    std::cout << "Image Processing Widget Opened" << std::endl;

    this->show();
}

//////////////////////////////////////////////////

void Image_Processing_Widget::_updateContrastFilter()
{
    if (_contrast_active) {
        auto media = _data_manager->getData<MediaData>("media");
        media->setProcess("1__lineartransform", std::bind(linear_transform, std::placeholders::_1, _contrast_alpha, _contrast_beta));
    }
}

void Image_Processing_Widget::_activateContrast()
{
    _contrast_active = ui->contrast_checkbox->isChecked();

    if (_contrast_active) {
        _updateContrastFilter();
    } else {
        auto media = _data_manager->getData<MediaData>("media");
        media->removeProcess("1__lineartransform");
    }
}

void Image_Processing_Widget::_updateContrastAlpha(){
    _contrast_alpha = ui->alpha_dspinbox->value();
    _updateContrastFilter();

    if (!_contrast_active)
    {
        ui->contrast_checkbox->setCheckState(Qt::Checked);
    }
}

void Image_Processing_Widget::_updateContrastBeta(){
    _contrast_beta = ui->beta_spinbox->value();
    _updateContrastFilter();

    if (!_contrast_active)
    {
        ui->contrast_checkbox->setCheckState(Qt::Checked);
    }
}

//////////////////////////////////////////////////

void Image_Processing_Widget::_updateGammaFilter()
{
    if (_gamma_active) {
        auto media = _data_manager->getData<MediaData>("media");
        media->setProcess("1__gamma", std::bind(gamma_transform, std::placeholders::_1, _gamma));
    }
}

void Image_Processing_Widget::_activateGamma()
{
    _gamma_active = ui->gamma_checkbox->isChecked();

    if (_gamma_active) {
        _updateGammaFilter();
    } else {
        auto media = _data_manager->getData<MediaData>("media");
        media->removeProcess("1__gamma");
    }
}

void Image_Processing_Widget::_updateGamma(){

    _gamma = ui->gamma_dspinbox->value();
    _updateGammaFilter();

    if (!_gamma_active)
    {
        ui->gamma_checkbox->setCheckState(Qt::Checked);
    }
}

//////////////////////////////////////////////////

void Image_Processing_Widget::_updateSharpenFilter()
{
    if (_sharpen_active) {
        auto media = _data_manager->getData<MediaData>("media");
        media->setProcess("2__sharpentransform", std::bind(sharpen_image, std::placeholders::_1, _sharpen_sigma));
    }
}

void Image_Processing_Widget::_activateSharpen()
{
    _sharpen_active = ui->sharpen_checkbox->isChecked();

    if (_sharpen_active) {
        _updateSharpenFilter();
    } else {
        auto media = _data_manager->getData<MediaData>("media");
        media->removeProcess("2__sharpentransform");
    }
}

void Image_Processing_Widget::_updateSharpenSigma()
{
    _sharpen_sigma = ui->sharpen_spinbox->value();
    _updateSharpenFilter();

    if (!_sharpen_active){
        ui->sharpen_checkbox->setCheckState(Qt::Checked);
    }
}

//////////////////////////////////////////////////

void Image_Processing_Widget::_updateClaheFilter()
{
    if (_clahe_active) {
        auto media = _data_manager->getData<MediaData>("media");
        media->setProcess("3__clahetransform", std::bind(clahe, std::placeholders::_1, _clahe_clip, _clahe_grid));
    }
}

void Image_Processing_Widget::_activateClahe()
{
    _clahe_active = ui->clahe_checkbox->isChecked();

    if (_clahe_active) {
        _updateClaheFilter();
    } else {
        auto media = _data_manager->getData<MediaData>("media");
        media->removeProcess("3__clahetransform");
    }
}

void Image_Processing_Widget::_updateClaheClip()
{
    _clahe_clip = ui->clahe_clip_spinbox->value();
    _updateClaheFilter();

    if (!_clahe_active) {
        ui->clahe_checkbox->setCheckState(Qt::Checked);
    }
}

void Image_Processing_Widget::_updateClaheGrid()
{
    _clahe_grid = ui->clahe_grid_spinbox->value();
    _updateClaheFilter();

    if (!_clahe_active) {
        ui->clahe_checkbox->setCheckState(Qt::Checked);
    }
}

//////////////////////////////////////////////////

void Image_Processing_Widget::_updateBilateralFilter()
{
    if (_bilateral_active) {
        auto media = _data_manager->getData<MediaData>("media");
        media->setProcess("4__bilateraltransform", std::bind(bilateral_filter,
                                                            std::placeholders::_1,
                                                            _bilateral_d,
                                                            _bilateral_color_sigma,
                                                            _bilateral_spatial_sigma));
    }
}

void Image_Processing_Widget::_activateBilateral()
{
    _bilateral_active = ui->bilateral_checkbox->isChecked();

    if (_bilateral_active) {
        _updateBilateralFilter();
    } else {
        auto media = _data_manager->getData<MediaData>("media");
        media->removeProcess("4__bilateraltransform");
    }
}

void Image_Processing_Widget::_updateBilateralD()
{
    _bilateral_d = ui->bilateral_d_spinbox->value();
    _updateBilateralFilter();

    if (!_bilateral_active) {
        ui->bilateral_checkbox->setCheckState(Qt::Checked);
    }
}

void Image_Processing_Widget::_updateBilateralSpatialSigma()
{
    _bilateral_spatial_sigma = ui->bilateral_spatial_spinbox->value();
    _updateBilateralFilter();

    if (!_bilateral_active) {
        ui->bilateral_checkbox->setCheckState(Qt::Checked);
    }
}
void Image_Processing_Widget::_updateBilateralColorSigma()
{
    _bilateral_color_sigma = ui->bilateral_color_spinbox->value();
    _updateBilateralFilter();

    if (!_bilateral_active) {
        ui->bilateral_checkbox->setCheckState(Qt::Checked);
    }
}
