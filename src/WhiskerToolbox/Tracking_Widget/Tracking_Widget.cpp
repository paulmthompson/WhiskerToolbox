
#include "Tracking_Widget.hpp"

#include "ui_Tracking_Widget.h"

#include "DataManager.hpp"
#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <iostream>

Tracking_Widget::Tracking_Widget(Media_Window *scene,
                               std::shared_ptr<DataManager> data_manager,
                               TimeScrollBar* time_scrollbar,
                               MainWindow* mainwindow,
                               QWidget *parent) :
        QMainWindow(parent),
        _scene{scene},
        _data_manager{data_manager},
        _time_scrollbar{time_scrollbar},
        _main_window{mainwindow},
        ui(new Ui::Tracking_Widget)
{
    ui->setupUi(this);

    _selected_scene = new QGraphicsScene();
    _selected_scene->setSceneRect(0, 0, 150, 150);

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();
}

Tracking_Widget::~Tracking_Widget() {
    delete ui;
}

void Tracking_Widget::openWidget() {

    std::cout << "Tracking Widget Opened" << std::endl;

    connect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));

    _data_manager->createPoint("tracking_point");

    _data_manager->getPoint("tracking_point")->setMaskHeight(_data_manager->getMediaData()->getHeight());
    _data_manager->getPoint("tracking_point")->setMaskWidth(_data_manager->getMediaData()->getWidth());

    _scene->addPointDataToScene("tracking_point");

    connect(ui->tableWidget, &QTableWidget::cellClicked, this, &Tracking_Widget::_tableClicked);

    this->show();
}

void Tracking_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

    disconnect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));
}

void Tracking_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {

    float x_media = x_canvas / _scene->getXAspect();
    float y_media = y_canvas / _scene->getYAspect();

    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    switch (_selection_mode) {

    case Tracking_Select: {
        std::string tracking_label =
            "(" + std::to_string(static_cast<int>(x_media)) + " , " + std::to_string(static_cast<int>(y_media)) +
            ")";
        ui->location_label->setText(QString::fromStdString(tracking_label));
        _data_manager->getPoint("tracking_point")->clearPointsAtTime(frame_id);
        _data_manager->getPoint("tracking_point")->addPointAtTime(frame_id, y_media, x_media);

        _buildContactTable();
        break;
    }
    default:
        break;
    }
}

void Tracking_Widget::_buildContactTable()
{

    auto point_data = _data_manager->getPoint("tracking_point");

    auto point_frames = point_data->getTimesWithPoints();

    ui->tableWidget->setRowCount(0);
    for (int i=0; i < point_frames.size(); i++)
    {
        auto point = point_data->getPointsAtTime(point_frames[i])[0];

        ui->tableWidget->insertRow(ui->tableWidget->rowCount());
        ui->tableWidget->setItem(i,0,new QTableWidgetItem(QString::number(point_frames[i])));
        ui->tableWidget->setItem(i,1,new QTableWidgetItem(QString::number(point.y)));
        ui->tableWidget->setItem(i,2,new QTableWidgetItem(QString::number(point.x)));

    }

    _highlighted_row = -1;
    //auto frame_id = _data_manager->getTime()->getLastLoadedFrame();
    //_updateContactWidgets(frame_id);
}

void Tracking_Widget::_tableClicked(int row, int column)
{
    if (column == 0 || column == 1) {
        int frame_id = ui->tableWidget->item(row, 0)->text().toInt();
        _time_scrollbar->changeScrollBarValue(frame_id);
    }

}
