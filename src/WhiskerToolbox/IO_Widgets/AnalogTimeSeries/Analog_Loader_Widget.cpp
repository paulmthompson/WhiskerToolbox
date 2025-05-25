
#include "Analog_Loader_Widget.hpp"

#include "ui_Analog_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"

#include <QFileDialog>


Analog_Loader_Widget::Analog_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Analog_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->load_from_csv, &QPushButton::clicked, this, &Analog_Loader_Widget::_loadAnalogCSV);
}

Analog_Loader_Widget::~Analog_Loader_Widget() {
    delete ui;
}

void Analog_Loader_Widget::_loadAnalogCSV() {
    auto analog_filename = QFileDialog::getOpenFileName(
            this,
            "Load Analog",
            QDir::currentPath(),
            "All files (*.*)");

    if (analog_filename.isNull()) {
        return;
    }

    /*
    auto const interval_key = ui->data_name_text->toPlainText().toStdString();

    char delimiter;
    if (ui->delimiter_combo->currentText() == "Space") {
        delimiter = ' ';
    } else if (ui->delimiter_combo->currentText() == "Comma") {
        delimiter = ',';
    } else {
        std::cout << "Unsupported delimiter" << std::endl;
        return;
    }

    auto intervals = load_digital_series_from_csv(interval_filename.toStdString(), delimiter);

    std::cout << "Loaded " << intervals.size() << " intervals" << std::endl;

    _data_manager->setData<DigitalIntervalSeries>(interval_key);

    _data_manager->getData<DigitalIntervalSeries>(interval_key)->setData(intervals);
    */
}
