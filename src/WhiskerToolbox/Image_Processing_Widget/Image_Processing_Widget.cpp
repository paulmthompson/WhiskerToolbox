
#include "Image_Processing_Widget.hpp"

#include "ui_Image_Processing_Widget.h"

#include "utils/opencv_utility.hpp"

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

    _updateContrastFilter();
}

void Image_Processing_Widget::_updateContrastFilter(){
    _data_manager->getMediaData()->insertProcess("1__lineartransform", std::bind(linear_transform, std::placeholders::_1, _contrast_alpha, _contrast_beta));
    _scene->UpdateCanvas();
}

void Image_Processing_Widget::openWidget() {

    std::cout << "Image Processing Widget Opened" << std::endl;

    this->show();
}

void Image_Processing_Widget::_updateContrastAlpha(){
    _contrast_alpha = ui->alpha_dspinbox->value();
    _updateContrastFilter();
}

void Image_Processing_Widget::_updateContrastBeta(){
    _contrast_beta = ui->beta_spinbox->value();
    _updateContrastFilter();
}
