
#include "Mask_Widget.hpp"
#include "ui_Mask_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/points.hpp"

#include "utils/Deep_Learning/Models/EfficientSAM/EfficientSAM.hpp"

#include <iostream>

Mask_Widget::Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Mask_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->load_sam_button, &QPushButton::clicked, this, &Mask_Widget::_loadSamModel);
}

Mask_Widget::~Mask_Widget() {
    delete ui;
}

void Mask_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
}

void Mask_Widget::selectPoint(float const x, float const y) {
    auto media = _data_manager->getData<MediaData>("media");
    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    auto const image_size = media->getImageSize();

    auto mask_image = _sam_model->process_frame(
            media->getProcessedData(current_time),
            image_size,
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)));

    std::vector<Point2D<float>> mask;
    for (size_t i = 0; i < mask_image.size(); i++) {
        if (mask_image[i] > 0) {
            mask.push_back(Point2D<float>{static_cast<float>(i % image_size.width),
                                          static_cast<float>(i / image_size.width)});
        }
    }

    _data_manager->getData<MaskData>(_active_key)->addMaskAtTime(current_time, mask);
}

void Mask_Widget::_loadSamModel() {

    _sam_model = std::make_unique<dl::EfficientSAM>();

    _sam_model->load_model();
}
