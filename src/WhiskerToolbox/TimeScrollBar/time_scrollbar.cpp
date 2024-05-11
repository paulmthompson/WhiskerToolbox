
#include "time_scrollbar.hpp"

#include "ui_time_scrollbar.h"

#include <QFileDialog>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

TimeScrollBar::TimeScrollBar(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    _data_manager{data_manager},
    ui(new Ui::TimeScrollBar)
{
    ui->setupUi(this);

};

TimeScrollBar::~TimeScrollBar() {
    delete ui;
}


