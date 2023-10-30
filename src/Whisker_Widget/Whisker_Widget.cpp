
#include "Whisker_Widget.h"
#include "qevent.h"

#include <QElapsedTimer>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>

void Whisker_Widget::createActions() {
    //connect(this,SIGNAL(this->show()),this,SLOT(openActions()));
    //connect(this,SIGNAL(this->close()),this,SLOT(closeActions()));
}

void Whisker_Widget::openWidget() {
    std::cout << "Whisker Widget Opened" << std::endl;

    connect(this->trace_button,SIGNAL(clicked()),this,SLOT(TraceButton()));
    connect(this->scene,SIGNAL(leftClick(qreal,qreal)),this,SLOT(ClickedInVideo(qreal,qreal)));

    connect(this->save_image,SIGNAL(clicked()),this,SLOT(SaveImageButton()));
    connect(this->save_whisker_mask,SIGNAL(clicked()),this,SLOT(SaveWhiskerMaskButton()));
    connect(this->contact_button,SIGNAL(clicked()),this,SLOT(ContactButton()));
    connect(this->save_contact_button,SIGNAL(clicked()),this,SLOT(SaveContact()));
    connect(this->load_contact_button,SIGNAL(clicked()),this,SLOT(LoadContact()));

    if (this->contact.empty()) {
        this->contact = std::vector<Contact>(this->time->getTotalFrameCount());
    }

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
    disconnect(this->save_whisker_mask,SIGNAL(clicked()),this,SLOT(SaveWhiskerMaskButton()));
    disconnect(this->contact_button,SIGNAL(clicked()),this,SLOT(ContactButton()));
    disconnect(this->save_contact_button,SIGNAL(clicked()),this,SLOT(SaveContact()));
    disconnect(this->load_contact_button,SIGNAL(clicked()),this,SLOT(LoadContact()));
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

void Whisker_Widget::SaveWhiskerMaskButton() {

    auto width = this->scene->getMediaWidth();
    auto height = this->scene->getMediaHeight();

    auto frame_id = this->time->getLastLoadedFrame();

    QImage mask_image(width,height,QImage::Format_Grayscale8);

    mask_image.fill(Qt::black);

    if (!this->wt->whiskers.empty()) {

        auto& w = this->wt->whiskers[this->selected_whisker-1];

        for (int i = 0; i < w.x.size(); i++) {

            auto x = std::lround(w.x[i]);
            auto y = std::lround(w.y[i]);

            mask_image.setPixelColor(x,y, Qt::white);
        }

    }

    std::stringstream ss;
    ss << std::setw(7) << std::setfill('0') << frame_id;

    std::string saveName = "w" +  ss.str() + ".png";
    std::cout << "Saving file " << saveName << std::endl;

    mask_image.save(QString::fromStdString(saveName));
}

void Whisker_Widget::ContactButton() {

    auto frame_num = this->time->getLastLoadedFrame();

    // If we are in a contact epoch, we need to mark the termination frame and add those to block
    if (this->contact_epoch) {

        this->contact_epoch = false;

        this->contact_button->setText("Mark Contact");

        for (int i = contact_start; i < frame_num; i++) {
            this->contact[i] = Contact::Contact;
        }

    } else {
        // If we are not already in contact epoch, start one
        this->contact_start = frame_num;

        this->contact_epoch = true;

        this->contact_button->setText("Mark Contact End");
    }
}

void Whisker_Widget::SaveContact() {


    std::fstream fout;

    fout.open("contact.csv",std::fstream::out);

    for (auto& frame_contact : this->contact) {
        if (frame_contact == Contact::Contact) {
            fout << "Contact" << "\n";
        } else {
            fout << "Nocontact" << "\n";
        }
    }

    fout.close();
}

void Whisker_Widget::LoadContact() {

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

