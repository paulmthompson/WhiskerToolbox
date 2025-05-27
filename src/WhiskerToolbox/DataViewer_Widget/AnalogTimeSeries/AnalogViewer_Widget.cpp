#include "AnalogViewer_Widget.hpp"
#include "ui_AnalogViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <iostream>

AnalogViewer_Widget::AnalogViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::AnalogViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
}

AnalogViewer_Widget::~AnalogViewer_Widget() {
    delete ui;
}

void AnalogViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    std::cout << "AnalogViewer_Widget: Active key set to " << key << std::endl;
} 