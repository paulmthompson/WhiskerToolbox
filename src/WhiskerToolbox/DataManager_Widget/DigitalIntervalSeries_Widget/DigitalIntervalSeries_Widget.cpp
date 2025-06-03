#include "DigitalIntervalSeries_Widget.hpp"
#include "ui_DigitalIntervalSeries_Widget.h"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "IO_Widgets/DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.hpp"
#include "IntervalTableModel.hpp"

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QItemDelegate>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

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
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->create_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_createIntervalButton);
    connect(ui->remove_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_removeIntervalButton);
    connect(ui->flip_single_frame, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_flipIntervalButton);
    connect(ui->tableView, &QTableView::doubleClicked, this, &DigitalIntervalSeries_Widget::_handleCellClicked);
    connect(ui->extend_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_extendInterval);

    // Interval operation connections
    connect(ui->merge_intervals_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_mergeIntervalsButton);
    connect(ui->tableView, &QTableView::customContextMenuRequested, this, &DigitalIntervalSeries_Widget::_showContextMenu);

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

    auto current_time = _data_manager->getCurrentTime();
    auto contactIntervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!contactIntervals) return;

    if (_interval_epoch) {
        _interval_epoch = false;
        ui->create_interval_button->setText("Create Interval");
        contactIntervals->addEvent(_interval_start, current_time);
    } else {
        _interval_start = current_time;
        _interval_epoch = true;
        ui->create_interval_button->setText("Mark Interval End");
    }
}

void DigitalIntervalSeries_Widget::_removeIntervalButton() {

    auto current_time = _data_manager->getCurrentTime();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    if (_interval_epoch) {
        _interval_epoch = false;
        ui->remove_interval_button->setText("Remove Interval");
        for (int i = _interval_start; i < current_time; i++) {
            intervals->setEventAtTime(i, false);
        }
    } else {
        _interval_start = current_time;
        _interval_epoch = true;
        ui->remove_interval_button->setText("Mark Remove Interval End");
    }
}

void DigitalIntervalSeries_Widget::_flipIntervalButton() {

    auto current_time = _data_manager->getCurrentTime();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    if (intervals->isEventAtTime(current_time)) {
        intervals->setEventAtTime(current_time, false);
    } else {
        intervals->setEventAtTime(current_time, true);
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

    auto current_time = _data_manager->getCurrentTime();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    for (auto const & index: selectedIndexes) {
        if (index.column() == 0) {
            Interval const interval = _interval_table_model->getInterval(index.row());
            if (current_time < interval.start) {
                intervals->addEvent(Interval{current_time, interval.end});
            } else if (current_time > interval.end) {
                intervals->addEvent(Interval{interval.start, current_time});
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
    if (options.filename.empty()) {
        QMessageBox::warning(this, "Filename Missing", "Please enter an output filename.");
        return;
    }
    IntervalSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void DigitalIntervalSeries_Widget::_initiateSaveProcess(SaverType saver_type, IntervalSaverOptionsVariant & options_variant) {
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
            CSVIntervalSaverOptions & specific_csv_options = std::get<CSVIntervalSaverOptions>(options_variant);
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
        save(interval_data_ptr.get(), options);
        return true;
    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save interval data (CSV): " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save interval data (CSV): " << e.what() << std::endl;
        return false;
    }
}

std::vector<Interval> DigitalIntervalSeries_Widget::_getSelectedIntervals() {
    std::vector<Interval> selected_intervals;
    QModelIndexList selected_indexes = ui->tableView->selectionModel()->selectedRows();

    for (QModelIndex const & index: selected_indexes) {
        if (index.isValid()) {
            Interval interval = _interval_table_model->getInterval(index.row());
            selected_intervals.push_back(interval);
        }
    }

    return selected_intervals;
}

void DigitalIntervalSeries_Widget::_showContextMenu(QPoint const & position) {
    QModelIndex index = ui->tableView->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const& target_key) {
        _moveIntervalsToTarget(target_key);
    };
    
    auto copy_callback = [this](std::string const& target_key) {
        _copyIntervalsToTarget(target_key);
    };

    add_move_copy_submenus<DigitalIntervalSeries>(&context_menu, _data_manager.get(), _active_key, move_callback, copy_callback);

    // Add separator and existing operations
    context_menu.addSeparator();
    QAction * merge_action = context_menu.addAction("Merge Selected Intervals");

    connect(merge_action, &QAction::triggered, this, &DigitalIntervalSeries_Widget::_mergeIntervalsButton);

    context_menu.exec(ui->tableView->mapToGlobal(position));
}

void DigitalIntervalSeries_Widget::_moveIntervalsToTarget(std::string const& target_key) {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.empty()) {
        std::cout << "No intervals selected to move." << std::endl;
        return;
    }

    auto source_interval_data = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    auto target_interval_data = _data_manager->getData<DigitalIntervalSeries>(target_key);

    if (!source_interval_data || !target_interval_data) {
        std::cerr << "Could not retrieve source or target DigitalIntervalSeries data." << std::endl;
        return;
    }

    // Add intervals to target
    for (Interval const & interval: selected_intervals) {
        target_interval_data->addEvent(interval);
    }

    // Remove intervals from source
    for (Interval const & interval: selected_intervals) {
        // Remove each time point in the interval from source
        for (int64_t time = interval.start; time <= interval.end; ++time) {
            source_interval_data->setEventAtTime(static_cast<int>(time), false);
        }
    }

    std::cout << "Moved " << selected_intervals.size() << " intervals from " << _active_key
              << " to " << target_key << std::endl;
}

void DigitalIntervalSeries_Widget::_copyIntervalsToTarget(std::string const& target_key) {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.empty()) {
        std::cout << "No intervals selected to copy." << std::endl;
        return;
    }

    auto target_interval_data = _data_manager->getData<DigitalIntervalSeries>(target_key);

    if (!target_interval_data) {
        std::cerr << "Could not retrieve target DigitalIntervalSeries data." << std::endl;
        return;
    }

    // Add intervals to target (source remains unchanged)
    for (Interval const & interval: selected_intervals) {
        target_interval_data->addEvent(interval);
    }

    std::cout << "Copied " << selected_intervals.size() << " intervals from " << _active_key
              << " to " << target_key << std::endl;
}

void DigitalIntervalSeries_Widget::_mergeIntervalsButton() {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.size() < 2) {
        std::cout << "Need at least 2 intervals selected to merge." << std::endl;
        return;
    }

    auto interval_data = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!interval_data) {
        std::cerr << "Could not retrieve DigitalIntervalSeries data." << std::endl;
        return;
    }

    // Find the overall range
    int64_t min_start = selected_intervals[0].start;
    int64_t max_end = selected_intervals[0].end;

    for (Interval const & interval: selected_intervals) {
        min_start = std::min(min_start, interval.start);
        max_end = std::max(max_end, interval.end);
    }

    // Remove all selected intervals first
    for (Interval const & interval: selected_intervals) {
        for (int64_t time = interval.start; time <= interval.end; ++time) {
            interval_data->setEventAtTime(static_cast<int>(time), false);
        }
    }

    // Add the merged interval
    interval_data->addEvent(Interval{min_start, max_end});

    std::cout << "Merged " << selected_intervals.size() << " intervals into range ["
              << min_start << ", " << max_end << "]" << std::endl;
}
