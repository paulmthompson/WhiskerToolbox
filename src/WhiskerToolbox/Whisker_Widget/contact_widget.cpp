
#include "contact_widget.hpp"

#include "ui_contact_widget.h"


Contact_Widget::Contact_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::contact_widget) {
    ui->setupUi(this);
};

Contact_Widget::~Contact_Widget() {
    delete ui;
}

void Contact_Widget::openWidget() {


    this->show();

}

void Contact_Widget::closeEvent(QCloseEvent *event) {


}
