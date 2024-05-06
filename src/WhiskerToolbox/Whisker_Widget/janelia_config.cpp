
#include "janelia_config.hpp"

#include "ui_janelia_config.h"


Janelia_Config::Janelia_Config(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::janelia_config) {
    ui->setupUi(this);
};

Janelia_Config::~Janelia_Config() {
    delete ui;
}

void Janelia_Config::openWidget() {

    this->show();

}

void Janelia_Config::closeEvent(QCloseEvent *event) {


}
