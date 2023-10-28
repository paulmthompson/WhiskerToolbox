
#include "Whisker_Widget.h"
#include "qevent.h"

#include <QElapsedTimer>

#include <iomanip>
#include <iostream>
#include <sstream>

void Whisker_Widget::createActions() {
    //connect(this,SIGNAL(this->show()),this,SLOT(openActions()));
    //connect(this,SIGNAL(this->close()),this,SLOT(closeActions()));
}

void Whisker_Widget::openWidget() {
    std::cout << "Whisker Widget Opened" << std::endl;

    connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    connect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));

    connect(this->save_image,SIGNAL(clicked()),this,SLOT(SaveImageButton()));

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

    disconnect(this->save_image,SIGNAL(clicked()),this,SLOT(SaveImageButton()));
}

void Whisker_Widget::TraceButton()
{
    QElapsedTimer timer2;
    timer2.start();

    this->wt->trace(this->scene->getCurrentFrame(),this->scene->getMediaHeight(), this->scene->getMediaWidth());

    int t1 = timer2.elapsed();
    DrawWhiskers();

    int t2 = timer2.elapsed();

    qDebug() << "The tracing took" << t1 << "ms and drawing took" << (t2-t1);
}

void Whisker_Widget::SaveImageButton()
{

    auto data = this->scene->getCurrentFrame();

    auto width = this->scene->getMediaWidth();
    auto height = this->scene->getMediaHeight();

    auto frame_id = this->time->getLastLoadedFrame();

    QImage labeled_image(&data[0],width,height, QImage::Format_Grayscale8);

    std::stringstream ss;
    ss << std::setw(7) << std::setfill('0') << frame_id;

    std::string saveName = "img" +  ss.str() + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    labeled_image.save(QString::fromStdString(saveName));

}

void Whisker_Widget::DrawWhiskers()
{
    scene->clearLines(); // We should have the scene do this every time a frame is advanced

    for (auto& w : wt->whiskers) {

        auto whisker_color = (w.id == this->selected_whisker) ? QPen(QColor(Qt::red)) : QPen(QColor(Qt::blue));

        scene->addLine(w.x,w.y,whisker_color);

    }
}

//x
void Whisker_Widget::ClickedInVideo(qreal x_canvas,qreal y_canvas) {

    float x_media = x_canvas / this->scene->getXAspect();
    float y_media = y_canvas / this->scene->getYAspect();

    switch(this->selection_mode) {
    case Whisker_Select: {
        std::tuple<float,int> nearest_whisker = wt->get_nearest_whisker(x_media, y_media);
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

