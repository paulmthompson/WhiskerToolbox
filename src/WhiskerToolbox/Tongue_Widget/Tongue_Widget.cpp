
#include "Tongue_Widget.hpp"

#include "ui_Tongue_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Media/Media_Data.hpp"

#include "Grabcut_Widget/Grabcut_Widget.hpp"
#include "TimeFrame.hpp"

#include <opencv2/opencv.hpp>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QPushButton>
#include "qevent.h"

#include <iostream>
#include <string>


Tongue_Widget::Tongue_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QMainWindow(parent),
      _data_manager{std::move(data_manager)},
    ui(new Ui::Tongue_Widget)
{
    ui->setupUi(this);
    connect(ui->begin_grabcut_btn, &QPushButton::clicked, this, &Tongue_Widget::_startGrabCut);
};

Tongue_Widget::~Tongue_Widget() {
    delete ui;
}

void Tongue_Widget::openWidget() {

    std::cout << "Tongue Widget Opened" << std::endl;

    this->show();
}

void Tongue_Widget::closeEvent(QCloseEvent *event) {

    static_cast<void>(event);

    std::cout << "Close event detected" << std::endl;

}

void Tongue_Widget::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    QMainWindow::keyPressEvent(event);

}

void Tongue_Widget::_startGrabCut(){
    auto media = _data_manager->getData<MediaData>("media");
    auto const current_time = _data_manager->getCurrentTime();
    auto media_data = media->getProcessedData(current_time);


    bool const is_gray = media->getFormat() == MediaData::DisplayFormat::Gray;
    cv::Mat img(
        media->getHeight(),
        media->getWidth(),
        is_gray ? CV_8UC1 : CV_8UC4,
        reinterpret_cast<cv::Scalar*>(media_data.data())
    );
    cv::cvtColor(img, img, is_gray ? cv::COLOR_GRAY2BGR : cv::COLOR_BGRA2BGR);

    if (!_grabcut_widget){
        _grabcut_widget = new Grabcut_Widget(_data_manager);
    }

    _grabcut_widget->setup(img, current_time);
    drawn.push_back(current_time);
    _grabcut_widget->openWidget();
}

