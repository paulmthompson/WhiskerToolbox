
#include "Tongue_Widget.hpp"

#include <QFileDialog>
#include <QElapsedTimer>
#include "qevent.h"

#include "ui_Tongue_Widget.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>



Tongue_Widget::Tongue_Widget(Media_Window *scene, std::shared_ptr<DataManager> data_manager, TimeScrollBar* time_scrollbar, QWidget *parent) :
    QMainWindow(parent),
    _scene{scene},
    _data_manager{data_manager},
    _time_scrollbar{time_scrollbar},
    ui(new Ui::Tongue_Widget)
{
    ui->setupUi(this);



};

Tongue_Widget::~Tongue_Widget() {
    delete ui;
}

void Tongue_Widget::openWidget() {

    std::cout << "Tongue Widget Opened" << std::endl;

    this->show();
}

void Tongue_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;

}

void Tongue_Widget::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    QMainWindow::keyPressEvent(event);

}
