
#include "Tracking_Widget.hpp"

#include "ui_Tracking_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

#include <fstream>
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
        ui(new Ui::Tracking_Widget),
        _output_path{std::filesystem::current_path()}
{
    ui->setupUi(this);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    _selected_scene = new QGraphicsScene();
    _selected_scene->setSceneRect(0, 0, 150, 150);

    ui->graphicsView->setScene(_selected_scene);
    ui->graphicsView->show();
}

Tracking_Widget::~Tracking_Widget() {
    delete ui;
}

void Tracking_Widget::openWidget() {

    std::cout << "Tracking Widget Opened" << std::endl;

    connect(_scene, SIGNAL(leftClick(qreal, qreal)), this, SLOT(_clickedInVideo(qreal, qreal)));

    _data_manager->setData<PointData>("tracking_point");

    auto media = _data_manager->getData<MediaData>("media");

    auto point = _data_manager->getData<PointData>("tracking_point");

    point->setMaskHeight(media->getHeight());
    point->setMaskWidth(media->getWidth());

    _scene->addPointDataToScene("tracking_point");

    connect(ui->tableWidget, &QTableWidget::cellClicked, this, &Tracking_Widget::_tableClicked);
    connect(ui->actionLoad_CSV, &QAction::triggered, this, &Tracking_Widget::_loadKeypointCSV);
    connect(ui->output_dir_button, &QPushButton::clicked, this, &Tracking_Widget::_changeOutputDir);
    connect(ui->save_csv_button, &QPushButton::clicked, this, &Tracking_Widget::_saveKeypointCSV);

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

        auto point = _data_manager->getData<PointData>("tracking_point");
        point->clearPointsAtTime(frame_id);
        point->addPointAtTime(frame_id, y_media, x_media);

        _buildContactTable();

        _scene->UpdateCanvas();
        break;
    }
    default:
        break;
    }
}

void Tracking_Widget::LoadFrame(int frame_id)
{

    if (ui->propagate_checkbox->isChecked())
    {
        _propagateLabel(frame_id);
    }


    auto points = _data_manager->getData<PointData>("tracking_point")->getPointsAtTime(frame_id);

    if (!points.empty()) {

        std::string x = "";
        std::string y = "";

        if (std::isnan(points[0].x))
        {
            x = "nan";
        } else {
            x = std::to_string(static_cast<int>(points[0].x));
        }

        if (std::isnan(points[0].y))
        {
            y="nan";
        } else {
            y = std::to_string(static_cast<int>(points[0].y));
        }

        std::string tracking_label =
            "(" + x + " , " + y + ")";
        ui->location_label->setText(QString::fromStdString(tracking_label));
    }
    _previous_frame = frame_id;
}

void Tracking_Widget::_buildContactTable()
{

    auto point_data = _data_manager->getData<PointData>("tracking_point");

    //auto point_frames = point_data->getTimesWithPoints();

    /*

    //ui->tableWidget->setRowCount(0);
    ui->tableWidget->setRowCount(point_frames.size());
    for (int i=0; i < point_frames.size(); i++)
    {
        auto point = point_data->getPointsAtTime(point_frames[i])[0];

        //ui->tableWidget->insertRow(ui->tableWidget->rowCount());
        ui->tableWidget->setItem(i,0,new QTableWidgetItem(QString::number(point_frames[i])));
        ui->tableWidget->setItem(i,1,new QTableWidgetItem(QString::number(point.y)));
        ui->tableWidget->setItem(i,2,new QTableWidgetItem(QString::number(point.x)));

    }

    */

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

void Tracking_Widget::_propagateLabel(int frame_id)
{

    auto prev_points = _data_manager->getData<PointData>("tracking_point")->getPointsAtTime(_previous_frame);

    for (int i = _previous_frame + 1; i <= frame_id; i ++)
    {
        _data_manager->getData<PointData>("tracking_point")->clearPointsAtTime(i);
        _data_manager->getData<PointData>("tracking_point")->addPointAtTime(i, prev_points[0].x, prev_points[0].y);
    }

    _scene->UpdateCanvas();
}

void Tracking_Widget::_loadKeypointCSV()
{
    auto keypoint_filename = QFileDialog::getOpenFileName(
        this,
        "Load Keypoints",
        QDir::currentPath(),
        "All files (*.*)");

    if (keypoint_filename.isNull()) {
        return;
    }

    const auto keypoint_key = "tracking_point";

    auto keypoints = load_points_from_csv(keypoint_filename.toStdString(), 0, 1, 2, ',');

    std::cout << "Loaded " << keypoints.size() << " keypoints" << std::endl;
    auto point_num = _data_manager->getKeys<PointData>().size();

    //_data_manager->createPoint(keypoint_key);

    auto point = _data_manager->getData<PointData>(keypoint_key);
    auto media = _data_manager->getData<MediaData>("media");

    point->setMaskHeight(media->getHeight());
    point->setMaskWidth(media->getWidth());

    for (auto & [key, val] : keypoints) {
        point->addPointAtTime(media->getFrameIndexFromNumber(key), val.x, val.y);
    }

    _scene->addPointDataToScene(keypoint_key);

}

void Tracking_Widget::_changeOutputDir()
{
    QString dir_name = QFileDialog::getExistingDirectory(
        this,
        "Select Directory",
        QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    _output_path = std::filesystem::path(dir_name.toStdString());
    ui->output_dir_label->setText(dir_name);
}

void Tracking_Widget::_saveKeypointCSV() {

    std::fstream fout;

    auto frame_by_frame_output = _output_path;

    fout.open(frame_by_frame_output.append("keypoint.csv").string(), std::fstream::out);

    auto point_data =_data_manager->getData<PointData>("tracking_point")->getData();

    for (auto& [key, val] : point_data)
    {
        fout << key << "," << std::to_string(val[0].x) << "," << std::to_string(val[0].y) << "\n";
    }

    fout.close();

}
