#include "EventViewer_Widget.hpp"
#include "ui_EventViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <iostream>

EventViewer_Widget::EventViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
}

EventViewer_Widget::~EventViewer_Widget() {
    delete ui;
}

void EventViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    std::cout << "EventViewer_Widget: Active key set to " << key << std::endl;
} 