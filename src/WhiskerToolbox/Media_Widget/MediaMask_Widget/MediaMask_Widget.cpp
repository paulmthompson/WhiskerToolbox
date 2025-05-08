#include "MediaMask_Widget.hpp"
#include "ui_MediaMask_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"

#include <iostream>

MediaMask_Widget::MediaMask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaMask_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

}

MediaMask_Widget::~MediaMask_Widget() {
    delete ui;
}

void MediaMask_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
}
