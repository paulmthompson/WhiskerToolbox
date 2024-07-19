
#include "Media_Widget.hpp"

#include "ui_Media_Widget.h"

#include "Media_Window/Media_Window.hpp"

Media_Widget::Media_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Media_Widget)
{
    ui->setupUi(this);
}

Media_Widget::~Media_Widget() {
    delete ui;
}

void Media_Widget::updateMedia() {

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

}
