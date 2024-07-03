
#include "Image_Processing_Widget.hpp"

#include "ui_Image_Processing_Widget.h"
#include "utils/opencv_utility.hpp"

#include <iostream>
#include <functional>

Image_Processing_Widget::Image_Processing_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QMainWindow(parent),
    _data_manager{data_manager},
    ui(new Ui::Image_Processing_Widget)
{
    ui->setupUi(this);


    connect(ui->alpha_dspinbox, &QDoubleSpinBox::valueChanged, this, &Image_Processing_Widget::_updAlpha);
    connect(ui->beta_spinbox, &QSpinBox::valueChanged, this, &Image_Processing_Widget::_updBeta);

    _updateFilters();

    // _data_manager->getMediaData()->insertProcess("1__lineartransform", [this](cv::Mat& mat){
    //     return linear_transform(mat, this->_alpha, this->_beta);
    // });
}
    //connect(ui->load_hdf_btn, &QPushButton::clicked, this, &Tongue_Widget::_loadHDF5TongueMasks);

void Image_Processing_Widget::_updateFilters(){
    this->_data_manager->getMediaData()->setProcess("1__lineartransform", std::bind(linear_transform, std::placeholders::_1, this->_alpha, this->_beta));
}

void Image_Processing_Widget::openWidget() {

    std::cout << "Image Processing Widget Opened" << std::endl;

    this->show();
}

void Image_Processing_Widget::_updAlpha(){
    this->_alpha = ui->alpha_dspinbox->value();
    //std::cout << "alpha=" << _alpha << '\n';
    //std::cout.flush();
    this->_updateFilters();
}

void Image_Processing_Widget::_updBeta(){
    this->_beta = ui->beta_spinbox->value();
    //std::cout << "beta=" << _beta << '\n';
    //std::cout.flush();
    this->_updateFilters();
}
