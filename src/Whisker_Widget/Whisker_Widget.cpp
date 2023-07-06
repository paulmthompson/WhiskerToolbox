
#include "Whisker_Widget.h"
#include "qevent.h"

#include <iostream>


void Whisker_Widget::createActions() {
    //connect(this,SIGNAL(this->show()),this,SLOT(openActions()));
    //connect(this,SIGNAL(this->close()),this,SLOT(closeActions()));
}

void Whisker_Widget::openWidget() {
    std::cout << "Whisker Widget Opened" << std::endl;

    this->show();

}

void Whisker_Widget::openActions() {
    std::cout << "Setting up actions for whisker tracker" << std::endl;
}

void Whisker_Widget::closeActions() {
    std::cout << "Disconnecting actions for whisker tracker" << std::endl;
}


void Whisker_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
}
