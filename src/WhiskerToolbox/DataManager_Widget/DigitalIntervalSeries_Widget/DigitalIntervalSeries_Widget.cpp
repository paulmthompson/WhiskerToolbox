#include "DigitalIntervalSeries_Widget.hpp"
#include "ui_DigitalIntervalSeries_Widget.h"

#include "../../DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "IntervalTableModel.hpp"
#include "IO_Widgets/DigitalIntervalLoaderWidget/CSV/CSVIntervalSaver_Widget.hpp"

#include <QEvent>
#include <QItemDelegate>
#include <QKeyEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>

#include <filesystem>
#include <iostream>


DigitalIntervalSeries_Widget::DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DigitalIntervalSeries_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _interval_table_model = new IntervalTableModel(this);
    ui->tableView->setModel(_interval_table_model);

    ui->tableView->setEditTriggers(QAbstractItemView::SelectedClicked);

    connect(ui->create_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_createIntervalButton);
    connect(ui->remove_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_removeIntervalButton);
    connect(ui->flip_single_frame, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_flipIntervalButton);
    connect(ui->tableView, &QTableView::doubleClicked, this, &DigitalIntervalSeries_Widget::_handleCellClicked);
    connect(ui->extend_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_extendInterval);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalIntervalSeries_Widget::_onExportTypeChanged);
    connect(ui->csv_interval_saver_widget, &CSVIntervalSaver_Widget::saveIntervalCSVRequested,
            this, &DigitalIntervalSeries_Widget::_handleSaveIntervalCSVRequested);

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
}

DigitalIntervalSeries_Widget::~DigitalIntervalSeries_Widget() {
    delete ui;
}

void DigitalIntervalSeries_Widget::openWidget() {
    this->show();
}

void DigitalIntervalSeries_Widget::setActiveKey(std::string key) {
    _active_key = std::move(key);

    _data_manager->removeCallbackFromData(_active_key, _callback_id);

    _assignCallbacks();

    _calculateIntervals();
}

void DigitalIntervalSeries_Widget::_changeDataTable(QModelIndex const & topLeft, QModelIndex const & bottomRight, QVector<int> const & roles) {

    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        Interval const interval = _interval_table_model->getInterval(row);
        std::cout << "Interval changed to " << interval.start << " , " << interval.end << std::endl;
        intervals->addEvent(interval.start, interval.end);
    }
}

void DigitalIntervalSeries_Widget::_assignCallbacks() {
    _callback_id = _data_manager->addCallbackToData(_active_key, [this]() {
        _calculateIntervals();
    });
}

void DigitalIntervalSeries_Widget::removeCallbacks() {
    if (!_active_key.empty() && _callback_id != -1) {
        _data_manager->removeCallbackFromData(_active_key, _callback_id);
        _callback_id = -1;
    }
}

void DigitalIntervalSeries_Widget::_calculateIntervals() {
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (intervals) {
        ui->total_interval_label->setText(QString::number(intervals->size()));
        _interval_table_model->setIntervals(intervals->getDigitalIntervalSeries());
    } else {
        ui->total_interval_label->setText("0");
        _interval_table_model->setIntervals({});
    }
}


void DigitalIntervalSeries_Widget::_createIntervalButton() {
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!contactIntervals) return;

    if (_interval_epoch) {
        _interval_epoch = false;
        ui->create_interval_button->setText("Create Interval");
        contactIntervals->addEvent(_interval_start, frame_num);
    } else {
        _interval_start = frame_num;
        _interval_epoch = true;
        ui->create_interval_button->setText("Mark Interval End");
    }
}

void DigitalIntervalSeries_Widget::_removeIntervalButton() {
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    if (_interval_epoch) {
        _interval_epoch = false;
        ui->remove_interval_button->setText("Remove Interval");
        for (int i = _interval_start; i < frame_num; i++) {
            intervals->setEventAtTime(i, false);
        }
    } else {
        _interval_start = frame_num;
        _interval_epoch = true;
        ui->remove_interval_button->setText("Mark Remove Interval End");
    }
}

void DigitalIntervalSeries_Widget::_flipIntervalButton() {
    auto frame_num = _data_manager->getTime()->getLastLoadedFrame();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    if (intervals->isEventAtTime(frame_num)) {
        intervals->setEventAtTime(frame_num, false);
    } else {
        intervals->setEventAtTime(frame_num, true);
    }
}

void DigitalIntervalSeries_Widget::_handleCellClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }
    int const frameNumber = index.data().toInt();
    emit frameSelected(frameNumber);
}

void DigitalIntervalSeries_Widget::_extendInterval() {
    auto selectedIndexes = ui->tableView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        std::cout << "Error: No frame selected in the table view." << std::endl;
        return;
    }

    auto currentFrame = _data_manager->getTime()->getLastLoadedFrame();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    for (auto const & index: selectedIndexes) {
        if (index.column() == 0) {
            Interval const interval = _interval_table_model->getInterval(index.row());
            if (currentFrame < interval.start) {
                intervals->addEvent(Interval{currentFrame, interval.end});
            } else if (currentFrame > interval.end) {
                intervals->addEvent(Interval{interval.start, currentFrame});
            } else {
                std::cout << "Error: Current frame is within the selected interval." << std::endl;
            }
        }
    }
}

void DigitalIntervalSeries_Widget::_onExportTypeChanged(int index) {
    QString current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_interval_saver_widget);
    } else {
        // Handle other export types if added in the future
    }
}

void DigitalIntervalSeries_Widget::_handleSaveIntervalCSVRequested(CSVIntervalSaverOptions options) {
    options.filename = ui->filename_edit->text().toStdString();
    if (options.filename.empty()){
        QMessageBox::warning(this, "Filename Missing", "Please enter an output filename.");
        return;
    }
    IntervalSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void DigitalIntervalSeries_Widget::_initiateSaveProcess(SaverType saver_type, IntervalSaverOptionsVariant& options_variant) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a DigitalIntervalSeries item to save.");
        return;
    }

    auto interval_data_ptr = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!interval_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve DigitalIntervalSeries for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    bool save_successful = false;
    switch (saver_type) {
        case SaverType::CSV: {
            CSVIntervalSaverOptions& specific_csv_options = std::get<CSVIntervalSaverOptions>(options_variant);
            specific_csv_options.parent_dir = _data_manager->getOutputPath().string(); 
            if (specific_csv_options.parent_dir.empty()) {
                 specific_csv_options.parent_dir = ".";
            }
            save_successful = _performActualCSVSave(specific_csv_options);
            break;
        }
        // Future saver types can be added here
    }

    if (save_successful) {
        QMessageBox::information(this, "Save Successful", QString::fromStdString("Interval data saved to " + std::get<CSVIntervalSaverOptions>(options_variant).parent_dir + "/" + std::get<CSVIntervalSaverOptions>(options_variant).filename));
        std::cout << "Interval data saved to: " << std::get<CSVIntervalSaverOptions>(options_variant).parent_dir << "/" << std::get<CSVIntervalSaverOptions>(options_variant).filename << std::endl;
    } else {
        QMessageBox::critical(this, "Save Error", "Failed to save interval data.");
    }
}

bool DigitalIntervalSeries_Widget::_performActualCSVSave(CSVIntervalSaverOptions & options) {
    auto interval_data_ptr = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!interval_data_ptr) {
        std::cerr << "_performActualCSVSave: Critical - Could not get DigitalIntervalSeries for key: " << _active_key << std::endl;
        return false;
    }

    try {
        save_digital_interval_series_to_csv(interval_data_ptr.get(), options);
        return true;
    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save interval data (CSV): " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save interval data (CSV): " << e.what() << std::endl;
        return false;
    }
}
