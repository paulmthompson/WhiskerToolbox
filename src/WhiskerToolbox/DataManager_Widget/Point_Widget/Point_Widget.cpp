#include "Point_Widget.hpp"
#include "ui_Point_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "PointTableModel.hpp"

Point_Widget::Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Point_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    _point_table_model = new PointTableModel(this);
    ui->tableView->setModel(_point_table_model);
}

Point_Widget::~Point_Widget() {
    delete ui;
}

void Point_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();
}

void Point_Widget::setActiveKey(const std::string &key)
{
    _active_key = key;
    auto points = _data_manager->getData<PointData>(_active_key)->getData();
    _point_table_model->setPoints(points);
}
