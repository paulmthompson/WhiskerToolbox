
#include "NewDataWidget.hpp"

#include "ui_NewDataWidget.h"


NewDataWidget::NewDataWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::NewDataWidget){
    ui->setupUi(this);


    connect(ui->new_data_button, &QPushButton::clicked, this, &NewDataWidget::_createNewData);

}

NewDataWidget::~NewDataWidget() {
    delete ui;
}

/*
void OutputDirectoryWidget::setDirLabel(QString const label) {
    ui->output_dir_label->setText(label);
}
*/
void NewDataWidget::_createNewData() {

    auto key = ui->new_data_name->toPlainText().toStdString();

    auto type = ui->new_data_type_combo->currentText().toStdString();

    emit createNewData(key, type);

}

