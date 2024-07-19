

#include "Media_Widget_Items.hpp"

#include "ui_Media_Widget_Items.h"


Media_Widget_Items::Media_Widget_Items(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Media_Widget_Items)
{
    ui->setupUi(this);
}

Media_Widget_Items::~Media_Widget_Items() {
    delete ui;
}

