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
    if (_data_manager && _data_manager_observer_id >= 0) {
        _data_manager->removeObserver(_data_manager_observer_id);
        _data_manager_observer_id = -1;
    }

    delete ui;
}

void NewDataWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    if (_data_manager && _data_manager_observer_id >= 0) {
        _data_manager->removeObserver(_data_manager_observer_id);
        _data_manager_observer_id = -1;
    }

    _data_manager = std::move(data_manager);

    if (_data_manager) {
        _data_manager_observer_id = _data_manager->addObserver([this]() {
            populateTimeframes();
        },
                                                               "NewDataWidget");
    }

    populateTimeframes();
}

void NewDataWidget::populateTimeframes() {
    auto const current_selection = ui->timeframe_combo->currentText();

    ui->timeframe_combo->clear();

    if (!_data_manager) {
        return;
    }

    auto timeframe_keys = _data_manager->getTimeFrameKeys();

    for (auto const & key: timeframe_keys) {
        ui->timeframe_combo->addItem(QString::fromStdString(key.str()));
    }

    if (!current_selection.isEmpty()) {
        int const selection_index = ui->timeframe_combo->findText(current_selection);
        if (selection_index >= 0) {
            ui->timeframe_combo->setCurrentIndex(selection_index);
            return;
        }
    }

    int const time_index = ui->timeframe_combo->findText("time");
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
