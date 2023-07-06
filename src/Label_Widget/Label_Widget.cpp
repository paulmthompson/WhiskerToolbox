
#include "Label_Widget.h"

#include <iostream>


void Label_Widget::openWidget() {
    std::cout << "Label Widget Opened" << std::endl;

    //connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    connect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));

    this->show();

}

void Label_Widget::closeEvent(QCloseEvent *event) {
    std::cout << "Close event detected" << std::endl;
    disconnect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));

}

void Label_Widget::ClickedInVideo(qreal x,qreal y) {

    this->label_maker->addLabel(this->scene->getLastLoadedFrame(), static_cast<int>(x), static_cast<int>(y));

    //update table

}
