
#include "Export_Video_Widget.hpp"

#include "ui_Export_Video_Widget.h"

#include "../../DataManager/DataManager.hpp"
#include "../../DataManager/Lines/Line_Data.hpp"

#include "Media_Window/Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "opencv2/opencv.hpp"
#include <QFileDialog>

#include <filesystem>
#include <iostream>
#include <regex>

Export_Video_Widget::Export_Video_Widget(
        std::shared_ptr<DataManager> data_manager,
        Media_Window * scene,
        TimeScrollBar * time_scrollbar,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Export_Video_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _time_scrollbar{time_scrollbar} {
    ui->setupUi(this);

    connect(ui->export_video_button, &QPushButton::clicked, this, &Export_Video_Widget::_exportVideo);

    ui->start_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());
    ui->end_frame_spinbox->setMaximum(_data_manager->getTime()->getTotalFrameCount());

    _video_writer = std::make_unique<cv::VideoWriter>();
}

Export_Video_Widget::~Export_Video_Widget() {
    delete ui;
}

void Export_Video_Widget::openWidget() {
    this->show();
}

void Export_Video_Widget::_exportVideo() {

    auto start_num = ui->start_frame_spinbox->value();

    auto end_num = ui->end_frame_spinbox->value();

    if (end_num == -1) {
        end_num = _data_manager->getTime()->getTotalFrameCount();
    }

    if (start_num >= end_num) {
        std::cout << "Start frame must be less than end frame" << std::endl;
    }

    auto filename = ui->output_filename->toPlainText().toStdString();

    // If filename doens't have .mp4 as ending, add it
    if (!std::regex_match(filename, std::regex(".*\\.mp4"))) {
        filename += ".mp4";
    }

    // Initialize the VideoWriter
    auto [width, height] = _scene->getCanvasSize();

    std::cout << "Initial height x width: " << width << " x " << height << std::endl;

    int const fps = 30;// Set the desired frame rate
    _video_writer->open(filename, cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, cv::Size(width, height), true);

    if (!_video_writer->isOpened()) {
        std::cout << "Could not open the output video file for write" << std::endl;
        return;
    }

    connect(_scene, &Media_Window::canvasUpdated, this, &Export_Video_Widget::_handleCanvasUpdated);

    for (int i = start_num; i < end_num; i++) {
        _time_scrollbar->changeScrollBarValue(i);
    }

    disconnect(_scene, &Media_Window::canvasUpdated, this, &Export_Video_Widget::_handleCanvasUpdated);

    _video_writer->release();
}

void Export_Video_Widget::_handleCanvasUpdated(QImage const & canvasImage) {
    auto frame = _data_manager->getTime()->getLastLoadedFrame();
    std::cout << "saving frame " << frame << std::endl;
    std::cout << canvasImage.height() << " x " << canvasImage.width() << std::endl;

    // Convert QImage to cv::Mat
    QImage convertedImage = canvasImage.convertToFormat(QImage::Format_RGB888);
    cv::Mat const mat(convertedImage.height(), convertedImage.width(), CV_8UC3, const_cast<uchar *>(convertedImage.bits()), convertedImage.bytesPerLine());
    cv::Mat matBGR;
    cv::cvtColor(mat, matBGR, cv::COLOR_RGB2BGR);

    // Write the frame to the video
    _video_writer->write(matBGR);
}
