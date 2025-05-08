#include "MediaPoint_Widget.hpp"
#include "ui_MediaPoint_Widget.h"

#include "DataManager/DataManager.hpp"
#include "Media_Window/Media_Window.hpp"

#include <iostream>

MediaPoint_Widget::MediaPoint_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaPoint_Widget),
      _data_manager{std::move(data_manager)},
      _scene{std::move(scene)} {
    ui->setupUi(this);
}

MediaPoint_Widget::~MediaPoint_Widget() {
    delete ui;
}

void MediaPoint_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
}
