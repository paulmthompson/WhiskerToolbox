

#include "Media_Widget_Items.hpp"

#include "ui_Media_Widget_Items.h"


Media_Widget_Items::Media_Widget_Items(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget *parent) :
    QWidget(parent),
    _data_manager{data_manager},
    _scene{scene},
    ui(new Ui::Media_Widget_Items)
{
    ui->setupUi(this);
}

Media_Widget_Items::~Media_Widget_Items() {
    delete ui;
}

