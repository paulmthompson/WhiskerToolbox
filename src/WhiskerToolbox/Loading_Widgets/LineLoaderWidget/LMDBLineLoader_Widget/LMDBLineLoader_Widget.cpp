
#include "LMDBLineLoader_Widget.hpp"

#include "ui_LMDBLineLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>

LMDBLineLoader_Widget::LMDBLineLoader_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::LMDBLineLoader_Widget){
    ui->setupUi(this);

}

LMDBLineLoader_Widget::~LMDBLineLoader_Widget() {
    delete ui;
}


