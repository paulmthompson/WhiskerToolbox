
#include "Whisker_Widget.h"
#include "qevent.h"

#include <QElapsedTimer>

#include <iostream>


void Whisker_Widget::createActions() {
    //connect(this,SIGNAL(this->show()),this,SLOT(openActions()));
    //connect(this,SIGNAL(this->close()),this,SLOT(closeActions()));
}

void Whisker_Widget::openWidget() {
    std::cout << "Whisker Widget Opened" << std::endl;

    connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    connect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));

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

    disconnect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    disconnect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));
}

void Whisker_Widget::TraceButton()
{
    QElapsedTimer timer2;
    timer2.start();

    this->wt->trace(this->scene->getCurrentFrame(),this->scene->getMediaDimensions());

    int t1 = timer2.elapsed();
    DrawWhiskers();

    int t2 = timer2.elapsed();

    qDebug() << "The tracing took" << t1 << "ms and drawing took" << (t2-t1);
}

void Whisker_Widget::DrawWhiskers()
{
    scene->clearLines(); // We should have the scene do this every time a frame is advanced

    for (auto& w : wt->whiskers) {

        auto whisker_color = (w.id == this->selected_whisker) ? QPen(QColor(Qt::red)) : QPen(QColor(Qt::blue));

        scene->addLine(w.x,w.y,whisker_color);

    }
}

void Whisker_Widget::ClickedInVideo(qreal x,qreal y) {

    switch(this->selection_mode) {
    case Whisker_Select: {
        std::tuple<float,int> nearest_whisker = wt->get_nearest_whisker(x, y);
        if (std::get<0>(nearest_whisker) < 10.0f) {
            this->selected_whisker = std::get<1>(nearest_whisker);
            this->DrawWhiskers();
        }
        break;
    }
    case Whisker_Pad_Select:

        break;

    default:
        break;
    }
}

