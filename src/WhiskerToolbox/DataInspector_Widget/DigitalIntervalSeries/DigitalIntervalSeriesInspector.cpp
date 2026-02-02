#include "DigitalIntervalSeriesInspector.hpp"
#include "ui_DigitalIntervalSeriesInspector.h"

#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/IO/formats/CSV/digitaltimeseries/Digital_Interval_Series_CSV.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "DataExport_Widget/DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.hpp"
#include "DigitalIntervalSeriesDataView.hpp"

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QItemDelegate>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QStackedWidget>

#include <algorithm>
#include <filesystem>
#include <iostream>

DigitalIntervalSeriesInspector::DigitalIntervalSeriesInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent),
      ui(new Ui::DigitalIntervalSeriesInspector) {
    ui->setupUi(this);

    // Initialize start frame label as hidden
    ui->start_frame_label->setVisible(false);

    _connectSignals();

    // Setup collapsible export section
    ui->export_section->autoSetContentLayout();
    ui->export_section->setTitle("Export Options");
    ui->export_section->toggle(false);// Start collapsed

    _onExportTypeChanged(ui->export_type_combo->currentIndex());

    // Set initial filename
    _updateFilename();
}

DigitalIntervalSeriesInspector::~DigitalIntervalSeriesInspector() {
    removeCallbacks();
    delete ui;
}

void DigitalIntervalSeriesInspector::setActiveKey(std::string const & key) {
    _active_key = key;

    if (!_active_key.empty() && _callback_id != -1) {
        dataManager()->removeCallbackFromData(_active_key, _callback_id);
    }

    // Reset interval creation state when switching to new key
    _cancelIntervalCreation();

    _assignCallbacks();

    _calculateIntervals();

    // Update filename based on new active key
    _updateFilename();
}

void DigitalIntervalSeriesInspector::removeCallbacks() {
    if (!_active_key.empty() && _callback_id != -1) {
        dataManager()->removeCallbackFromData(_active_key, _callback_id);
        _callback_id = -1;
    }

    // Cancel any ongoing interval creation
    _cancelIntervalCreation();
}

void DigitalIntervalSeriesInspector::updateView() {
    // DigitalIntervalSeriesInspector auto-updates through callbacks
    // No explicit updateTable method needed
}

void DigitalIntervalSeriesInspector::setDataView(DigitalIntervalSeriesDataView * view) {
    if (view) {
        // Set up selection provider to get selected intervals from the view
        setSelectionProvider([view]() {
            return view->getSelectedIntervals();
        });
    }
}

void DigitalIntervalSeriesInspector::setSelectionProvider(std::function<std::vector<Interval>()> provider) {
    _selection_provider = std::move(provider);
}

void DigitalIntervalSeriesInspector::_connectSignals() {
    connect(ui->create_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeriesInspector::_createIntervalButton);
    connect(ui->remove_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeriesInspector::_removeIntervalButton);
    connect(ui->flip_single_frame, &QPushButton::clicked, this, &DigitalIntervalSeriesInspector::_flipIntervalButton);
    connect(ui->extend_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeriesInspector::_extendInterval);

    // Interval operation connections
    connect(ui->merge_intervals_button, &QPushButton::clicked, this, &DigitalIntervalSeriesInspector::_mergeIntervalsButton);

    // New interval creation enhancements
    connect(ui->cancel_interval_button, &QPushButton::clicked, this, &DigitalIntervalSeriesInspector::_cancelIntervalButton);
    connect(ui->create_interval_button, &QPushButton::customContextMenuRequested, this, &DigitalIntervalSeriesInspector::_createIntervalContextMenuRequested);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalIntervalSeriesInspector::_onExportTypeChanged);
    connect(ui->csv_interval_saver_widget, &CSVIntervalSaver_Widget::saveIntervalCSVRequested,
            this, &DigitalIntervalSeriesInspector::_handleSaveIntervalCSVRequested);
}

void DigitalIntervalSeriesInspector::_assignCallbacks() {
    if (!_active_key.empty()) {
        _callback_id = dataManager()->addCallbackToData(_active_key, [this]() {
            _calculateIntervals();
        });
    }
}

void DigitalIntervalSeriesInspector::_calculateIntervals() {
    auto intervals = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (intervals) {
        ui->total_interval_label->setText(QString::number(intervals->size()));
    } else {
        ui->total_interval_label->setText("0");
    }
}

int64_t DigitalIntervalSeriesInspector::_getCurrentTimeInSeriesFrame() const {
    if (!state()) {
        return -1;
    }

    auto const & time_position = state()->current_position;
    if (!time_position.isValid() || !time_position.time_frame) {
        return -1;
    }

    auto intervals = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) {
        return -1;
    }

    auto series_timeframe = intervals->getTimeFrame();
    if (!series_timeframe) {
        return -1;
    }

    // Convert the state's time_position to the series' timeframe
    TimeFrameIndex converted_index = time_position.convertTo(series_timeframe);
    return converted_index.getValue();
}

void DigitalIntervalSeriesInspector::_createIntervalButton() {

    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalIntervalSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto contactIntervals = dataManager()->getData<DigitalIntervalSeries>(_active_key);
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

void DigitalIntervalSeriesInspector::_removeIntervalButton() {

    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalIntervalSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto intervals = dataManager()->getData<DigitalIntervalSeries>(_active_key);
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

void DigitalIntervalSeriesInspector::_flipIntervalButton() {

    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalIntervalSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto intervals = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    auto series_timeframe = intervals->getTimeFrame();
    if (!series_timeframe) {
        std::cerr << "DigitalIntervalSeriesInspector: Series has no TimeFrame" << std::endl;
        return;
    }

    TimeFrameIndex series_index(current_time);
    if (intervals->hasIntervalAtTime(series_index, *series_timeframe)) {
        intervals->setEventAtTime(series_index, false);
    } else {
        intervals->setEventAtTime(series_index, true);
    }
}

void DigitalIntervalSeriesInspector::_extendInterval() {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.empty()) {
        std::cout << "Error: No intervals selected in the view panel." << std::endl;
        return;
    }

    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalIntervalSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto intervals = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (!intervals) return;

    for (Interval const & interval : selected_intervals) {
        if (current_time < interval.start) {
            intervals->addEvent(Interval{current_time, interval.end});
        } else if (current_time > interval.end) {
            intervals->addEvent(Interval{interval.start, current_time});
        } else {
            std::cout << "Error: Current frame is within the selected interval." << std::endl;
        }
    }
}

void DigitalIntervalSeriesInspector::_onExportTypeChanged(int index) {
    QString current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_interval_saver_widget);
    } else {
        // Handle other export types if added in the future
    }

    // Update filename based on new export type
    _updateFilename();
}

void DigitalIntervalSeriesInspector::_handleSaveIntervalCSVRequested(CSVIntervalSaverOptions options) {
    options.filename = ui->filename_edit->text().toStdString();
    if (options.filename.empty()) {
        QMessageBox::warning(this, "Filename Missing", "Please enter an output filename.");
        return;
    }
    IntervalSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void DigitalIntervalSeriesInspector::_initiateSaveProcess(SaverType saver_type, IntervalSaverOptionsVariant & options_variant) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select a DigitalIntervalSeries item to save.");
        return;
    }

    auto interval_data_ptr = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (!interval_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve DigitalIntervalSeries for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    bool save_successful = false;
    switch (saver_type) {
        case SaverType::CSV: {
            CSVIntervalSaverOptions & specific_csv_options = std::get<CSVIntervalSaverOptions>(options_variant);
            specific_csv_options.parent_dir = dataManager()->getOutputPath();
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

bool DigitalIntervalSeriesInspector::_performActualCSVSave(CSVIntervalSaverOptions & options) {
    auto interval_data_ptr = dataManager()->getData<DigitalIntervalSeries>(_active_key);
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

std::vector<Interval> DigitalIntervalSeriesInspector::_getSelectedIntervals() {
    if (_selection_provider) {
        return _selection_provider();
    }
    return {};
}

void DigitalIntervalSeriesInspector::_moveIntervalsToTarget(std::string const & target_key) {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.empty()) {
        std::cout << "No intervals selected to move." << std::endl;
        return;
    }

    auto source_interval_data = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    auto target_interval_data = dataManager()->getData<DigitalIntervalSeries>(target_key);

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

void DigitalIntervalSeriesInspector::_copyIntervalsToTarget(std::string const & target_key) {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.empty()) {
        std::cout << "No intervals selected to copy." << std::endl;
        return;
    }

    auto target_interval_data = dataManager()->getData<DigitalIntervalSeries>(target_key);

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

void DigitalIntervalSeriesInspector::_mergeIntervalsButton() {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.size() < 2) {
        std::cout << "Need at least 2 intervals selected to merge." << std::endl;
        return;
    }

    auto interval_data = dataManager()->getData<DigitalIntervalSeries>(_active_key);
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

void DigitalIntervalSeriesInspector::_updateStartFrameLabel(int64_t frame_number) {
    if (frame_number == -1) {
        ui->start_frame_label->setText("");
        ui->start_frame_label->setVisible(false);
    } else {
        ui->start_frame_label->setText(QString("Start: %1").arg(frame_number));
        ui->start_frame_label->setVisible(true);
    }
}

void DigitalIntervalSeriesInspector::_cancelIntervalCreation() {
    if (_interval_epoch) {
        _interval_epoch = false;
        ui->create_interval_button->setText("Create Interval");
        ui->cancel_interval_button->setVisible(false);
        _updateStartFrameLabel(-1);

        std::cout << "Interval creation cancelled" << std::endl;
    }
}

void DigitalIntervalSeriesInspector::_cancelIntervalButton() {
    _cancelIntervalCreation();
}

void DigitalIntervalSeriesInspector::_createIntervalContextMenuRequested(QPoint const & position) {
    _showCreateIntervalContextMenu(position);
}

void DigitalIntervalSeriesInspector::_showCreateIntervalContextMenu(QPoint const & position) {
    // Only show context menu if we're in interval creation mode
    if (!_interval_epoch) {
        return;
    }

    QMenu context_menu(this);
    QAction * cancel_action = context_menu.addAction("Cancel Interval Creation");

    connect(cancel_action, &QAction::triggered, this, &DigitalIntervalSeriesInspector::_cancelIntervalCreation);

    context_menu.exec(ui->create_interval_button->mapToGlobal(position));
}

std::string DigitalIntervalSeriesInspector::_generateFilename() const {
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

void DigitalIntervalSeriesInspector::_updateFilename() {
    std::string filename = _generateFilename();
    ui->filename_edit->setText(QString::fromStdString(filename));
}

void DigitalIntervalSeriesInspector::_deleteSelectedIntervals() {
    std::vector<Interval> selected_intervals = _getSelectedIntervals();
    if (selected_intervals.empty()) {
        std::cout << "DigitalIntervalSeriesInspector: No intervals selected to delete." << std::endl;
        return;
    }

    auto interval_data_ptr = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (!interval_data_ptr) {
        std::cerr << "DigitalIntervalSeriesInspector: DigitalIntervalSeries object ('" << _active_key << "') not found." << std::endl;
        return;
    }

    std::cout << "DigitalIntervalSeriesInspector: Deleting " << selected_intervals.size() 
              << " intervals from '" << _active_key << "'..." << std::endl;

    // Delete the selected intervals
    size_t deleted_count = interval_data_ptr->removeIntervals(selected_intervals);

    if (deleted_count > 0) {
        std::cout << "DigitalIntervalSeriesInspector: Successfully deleted " << deleted_count 
                  << " intervals." << std::endl;
        // The table will be automatically updated through the observer pattern
    } else {
        std::cout << "DigitalIntervalSeriesInspector: No intervals were deleted." << std::endl;
    }
}
