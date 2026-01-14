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

#include <algorithm>
#include <filesystem>
#include <iostream>


DigitalIntervalSeries_Widget::DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DigitalIntervalSeries_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _interval_table_model = new IntervalTableModel(this);
    ui->tableView->setModel(_interval_table_model);

    // Initialize start frame label as hidden
    ui->start_frame_label->setVisible(false);

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

    // New interval creation enhancements
    connect(ui->cancel_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_cancelIntervalButton);
    connect(ui->create_interval_button, &QPushButton::customContextMenuRequested, this, &DigitalIntervalSeries_Widget::_createIntervalContextMenuRequested);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalIntervalSeries_Widget::_onExportTypeChanged);
    connect(ui->csv_interval_saver_widget, &CSVIntervalSaver_Widget::saveIntervalCSVRequested,
            this, &DigitalIntervalSeries_Widget::_handleSaveIntervalCSVRequested);

    // Setup collapsible export section
    ui->export_section->autoSetContentLayout();
    ui->export_section->setTitle("Export Options");
    ui->export_section->toggle(false);// Start collapsed

    _onExportTypeChanged(ui->export_type_combo->currentIndex());

    // Set initial filename
    _updateFilename();
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

    // Reset interval creation state when switching to new key
    _cancelIntervalCreation();

    _assignCallbacks();

    _calculateIntervals();

    // Update filename based on new active key
    _updateFilename();
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

    // Cancel any ongoing interval creation
    _cancelIntervalCreation();
}

void DigitalIntervalSeries_Widget::_calculateIntervals() {
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (intervals) {
        ui->total_interval_label->setText(QString::number(intervals->size()));
        std::vector<Interval> interval_vector;
        for (auto const & interval_with_id: intervals->view()) {
            interval_vector.push_back(interval_with_id.value());
        }
        _interval_table_model->setIntervals(interval_vector);
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
        // User is selecting the second frame
        _interval_epoch = false;
        _interval_end = current_time;

        // Ensure proper ordering (start <= end) by swapping if necessary
        int64_t start = std::min(_interval_start, _interval_end);
        int64_t end = std::max(_interval_start, _interval_end);

        contactIntervals->addEvent(TimeFrameIndex(start), TimeFrameIndex(end));

        // Reset UI state
        ui->create_interval_button->setText("Create Interval");
        ui->cancel_interval_button->setVisible(false);
        _updateStartFrameLabel(-1);// Clear the label

    } else {
        // User is selecting the first frame
        _interval_start = current_time;
        _interval_epoch = true;

        // Update UI state
        ui->create_interval_button->setText("Mark Interval End");
        ui->cancel_interval_button->setVisible(true);
        _updateStartFrameLabel(_interval_start);
    }
}

void DigitalIntervalSeries_Widget::_removeIntervalButton() {

    auto current_time = _data_manager->getCurrentTime();
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    if (_interval_epoch) {
        _interval_epoch = false;
        ui->remove_interval_button->setText("Remove Interval");
        for (int64_t i = _interval_start; i < current_time; i++) {
            intervals->setEventAtTime(TimeFrameIndex(i), false);
        }
    } else {
        _interval_start = current_time;
        _interval_epoch = true;
        ui->remove_interval_button->setText("Mark Remove Interval End");
    }
}

void DigitalIntervalSeries_Widget::_flipIntervalButton() {

    auto current_time_and_frame = _data_manager->getCurrentIndexAndFrame(TimeKey("time"));
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    if (intervals->hasIntervalAtTime(current_time_and_frame.index,
                                    *current_time_and_frame.time_frame)) {
        intervals->setEventAtTime(current_time_and_frame.index, false);
    } else {
        intervals->setEventAtTime(current_time_and_frame.index, true);
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

    // Update filename based on new export type
    _updateFilename();
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
            specific_csv_options.parent_dir = _data_manager->getOutputPath();
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
    auto move_callback = [this](std::string const & target_key) {
        _moveIntervalsToTarget(target_key);
    };

    auto copy_callback = [this](std::string const & target_key) {
        _copyIntervalsToTarget(target_key);
    };

    add_move_copy_submenus<DigitalIntervalSeries>(&context_menu, _data_manager.get(), _active_key, move_callback, copy_callback);

    // Add separator and existing operations
    context_menu.addSeparator();
    QAction * merge_action = context_menu.addAction("Merge Selected Intervals");
    QAction * delete_action = context_menu.addAction("Delete Selected Intervals");

    connect(merge_action, &QAction::triggered, this, &DigitalIntervalSeries_Widget::_mergeIntervalsButton);
    connect(delete_action, &QAction::triggered, this, &DigitalIntervalSeries_Widget::_deleteSelectedIntervals);

    context_menu.exec(ui->tableView->mapToGlobal(position));
}

void DigitalIntervalSeries_Widget::_moveIntervalsToTarget(std::string const & target_key) {
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
            source_interval_data->setEventAtTime(TimeFrameIndex(time), false);
        }
    }

    std::cout << "Moved " << selected_intervals.size() << " intervals from " << _active_key
              << " to " << target_key << std::endl;
}

void DigitalIntervalSeries_Widget::_copyIntervalsToTarget(std::string const & target_key) {
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
            interval_data->setEventAtTime(TimeFrameIndex(time), false);
        }
    }

    // Add the merged interval
    interval_data->addEvent(Interval{min_start, max_end});

    std::cout << "Merged " << selected_intervals.size() << " intervals into range ["
              << min_start << ", " << max_end << "]" << std::endl;
}

void DigitalIntervalSeries_Widget::_updateStartFrameLabel(int64_t frame_number) {
    if (frame_number == -1) {
        ui->start_frame_label->setText("");
        ui->start_frame_label->setVisible(false);
    } else {
        ui->start_frame_label->setText(QString("Start: %1").arg(frame_number));
        ui->start_frame_label->setVisible(true);
    }
}

void DigitalIntervalSeries_Widget::_cancelIntervalCreation() {
    if (_interval_epoch) {
        _interval_epoch = false;
        ui->create_interval_button->setText("Create Interval");
        ui->cancel_interval_button->setVisible(false);
        _updateStartFrameLabel(-1);

        std::cout << "Interval creation cancelled" << std::endl;
    }
}

void DigitalIntervalSeries_Widget::_cancelIntervalButton() {
    _cancelIntervalCreation();
}

void DigitalIntervalSeries_Widget::_createIntervalContextMenuRequested(QPoint const & position) {
    _showCreateIntervalContextMenu(position);
}

void DigitalIntervalSeries_Widget::_showCreateIntervalContextMenu(QPoint const & position) {
    // Only show context menu if we're in interval creation mode
    if (!_interval_epoch) {
        return;
    }

    QMenu context_menu(this);
    QAction * cancel_action = context_menu.addAction("Cancel Interval Creation");

    connect(cancel_action, &QAction::triggered, this, &DigitalIntervalSeries_Widget::_cancelIntervalCreation);

    context_menu.exec(ui->create_interval_button->mapToGlobal(position));
}

std::string DigitalIntervalSeries_Widget::_generateFilename() const {
    if (_active_key.empty()) {
        return "intervals_output.csv";// Fallback default
    }

    // Sanitize the active key for filesystem safety
    std::string sanitized_key = _active_key;

    // Replace characters that might be problematic in filenames
    std::string invalid_chars = "<>:\"/\\|?*";
    for (char invalid_char: invalid_chars) {
        std::replace(sanitized_key.begin(), sanitized_key.end(), invalid_char, '_');
    }

    // Remove leading/trailing whitespace and dots
    auto start = sanitized_key.find_first_not_of(" \t\n\r\f\v.");
    auto end = sanitized_key.find_last_not_of(" \t\n\r\f\v.");
    if (start == std::string::npos) {
        sanitized_key = "intervals_output";// Fallback if only invalid chars
    } else {
        sanitized_key = sanitized_key.substr(start, end - start + 1);
    }

    QString current_export_type = ui->export_type_combo->currentText();
    std::string extension;

    if (current_export_type == "CSV") {
        extension = ".csv";
    } else {
        // Future export types can be added here
        // Example: if (current_export_type == "JSON") extension = ".json";
        extension = ".csv";// Default fallback
    }

    return sanitized_key + extension;
}

void DigitalIntervalSeries_Widget::_updateFilename() {
    std::string filename = _generateFilename();
    ui->filename_edit->setText(QString::fromStdString(filename));
}

void DigitalIntervalSeries_Widget::_deleteSelectedIntervals() {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.empty()) {
        std::cout << "DigitalIntervalSeries_Widget: No intervals selected to delete." << std::endl;
        return;
    }

    auto interval_data_ptr = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    if (!interval_data_ptr) {
        std::cerr << "DigitalIntervalSeries_Widget: DigitalIntervalSeries object ('" << _active_key << "') not found." << std::endl;
        return;
    }

    std::cout << "DigitalIntervalSeries_Widget: Deleting " << selected_intervals.size() 
              << " intervals from '" << _active_key << "'..." << std::endl;

    // Delete the selected intervals
    size_t deleted_count = interval_data_ptr->removeIntervals(selected_intervals);

    if (deleted_count > 0) {
        std::cout << "DigitalIntervalSeries_Widget: Successfully deleted " << deleted_count 
                  << " intervals." << std::endl;
        // The table will be automatically updated through the observer pattern
    } else {
        std::cout << "DigitalIntervalSeries_Widget: No intervals were deleted." << std::endl;
    }
}
