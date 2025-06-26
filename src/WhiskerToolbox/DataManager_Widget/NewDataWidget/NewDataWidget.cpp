#include "NewDataWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "ui_NewDataWidget.h"


NewDataWidget::NewDataWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::NewDataWidget) {
    ui->setupUi(this);

    connect(ui->new_data_button, &QPushButton::clicked, this, &NewDataWidget::_createNewData);
}

NewDataWidget::~NewDataWidget() {
    delete ui;
}

void NewDataWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = data_manager;
    // Populate timeframes when DataManager is set
    populateTimeframes();
}

void NewDataWidget::populateTimeframes() {
    ui->timeframe_combo->clear();

    if (!_data_manager) {
        return;
    }

    // Get available timeframe keys
    auto timeframe_keys = _data_manager->getTimeFrameKeys();

    for (auto const & key: timeframe_keys) {
        ui->timeframe_combo->addItem(QString::fromStdString(key));
    }

    // Set "time" as default if it exists
    int time_index = ui->timeframe_combo->findText("time");
    if (time_index >= 0) {
        ui->timeframe_combo->setCurrentIndex(time_index);
    }
}

/*
void OutputDirectoryWidget::setDirLabel(QString const label) {
    ui->output_dir_label->setText(label);
}
*/
void NewDataWidget::_createNewData() {

    auto key = ui->new_data_name->text().toStdString();
    auto type = ui->new_data_type_combo->currentText().toStdString();
    auto timeframe_key = ui->timeframe_combo->currentText().toStdString();

    emit createNewData(key, type, timeframe_key);
}
