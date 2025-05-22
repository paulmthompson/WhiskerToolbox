
#include "CSVLineLoader_Widget.hpp"

#include "ui_CSVLineLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>

CSVLineLoader_Widget::CSVLineLoader_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVLineLoader_Widget){
    ui->setupUi(this);

}

CSVLineLoader_Widget::~CSVLineLoader_Widget() {
    delete ui;
}


