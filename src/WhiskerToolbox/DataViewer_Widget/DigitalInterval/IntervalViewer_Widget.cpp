#include "IntervalViewer_Widget.hpp"
#include "ui_IntervalViewer_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataViewer_Widget/OpenGLWidget.hpp"

#include <iostream>

IntervalViewer_Widget::IntervalViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::IntervalViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget}
{
    ui->setupUi(this);
}

IntervalViewer_Widget::~IntervalViewer_Widget() {
    delete ui;
}

void IntervalViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    
    std::cout << "IntervalViewer_Widget: Active key set to " << key << std::endl;
} 