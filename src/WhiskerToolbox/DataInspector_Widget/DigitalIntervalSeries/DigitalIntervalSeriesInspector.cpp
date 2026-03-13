#include "DigitalIntervalSeriesInspector.hpp"
#include "ui_DigitalIntervalSeriesInspector.h"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "Commands/IO/SaveData.hpp"
#include "DataExport_Widget/DigitalTimeSeries/CSV/CSVIntervalSaver_Widget.hpp"
#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataInspector_Widget/Inspectors/GroupFilterHelper.hpp"
#include "DataManager.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/IO/SaveData.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "DigitalIntervalSeriesDataView.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

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
#include <unordered_set>

#include <rfl/json.hpp>

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

    // Initialize group filter combo box
    _populateGroupFilterCombo();

    // Set up group manager signals if group manager exists
    connectGroupManagerSignals(groupManager(), this, &DigitalIntervalSeriesInspector::_onGroupChanged);
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
    // Disconnect from old view if any
    if (_data_view) {
        disconnect(_data_view, &DigitalIntervalSeriesDataView::moveIntervalsRequested, this, nullptr);
        disconnect(_data_view, &DigitalIntervalSeriesDataView::copyIntervalsRequested, this, nullptr);
        disconnect(_data_view, &DigitalIntervalSeriesDataView::moveIntervalsToGroupRequested, this, nullptr);
        disconnect(_data_view, &DigitalIntervalSeriesDataView::removeIntervalsFromGroupRequested, this, nullptr);
        disconnect(_data_view, &DigitalIntervalSeriesDataView::deleteIntervalsRequested, this, nullptr);
    }

    _data_view = view;
    if (_data_view) {
        // Set up selection provider
        setSelectionProvider([view]() {
            return view->getSelectedIntervals();
        });

        // Set group manager on the view
        if (groupManager()) {
            _data_view->setGroupManager(groupManager());
        }

        // Connect to view signals for move/copy operations
        connect(_data_view, &DigitalIntervalSeriesDataView::moveIntervalsRequested,
                this, &DigitalIntervalSeriesInspector::_moveIntervalsToTarget);
        connect(_data_view, &DigitalIntervalSeriesDataView::copyIntervalsRequested,
                this, &DigitalIntervalSeriesInspector::_copyIntervalsToTarget);

        // Connect to view signals for group management operations
        connect(_data_view, &DigitalIntervalSeriesDataView::moveIntervalsToGroupRequested,
                this, &DigitalIntervalSeriesInspector::_moveIntervalsToGroup);
        connect(_data_view, &DigitalIntervalSeriesDataView::removeIntervalsFromGroupRequested,
                this, &DigitalIntervalSeriesInspector::_removeIntervalsFromGroup);

        // Connect to view signal for delete operation
        connect(_data_view, &DigitalIntervalSeriesDataView::deleteIntervalsRequested,
                this, &DigitalIntervalSeriesInspector::_deleteSelectedIntervals);
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
            this, [this](CSVIntervalSaverOptions options) {
                auto output_path = dataManager()->getOutputPath();
                if (output_path.empty()) {
                    QMessageBox::warning(this, "Warning",
                                         "Please set an output directory in the Data Manager settings");
                    return;
                }

                auto filename = ui->filename_edit->text().toStdString();
                auto const filepath = (std::filesystem::path(output_path) / filename).string();

                options.parent_dir = output_path;
                options.filename = filename;

                auto const opts_json = rfl::json::write(options);
                auto format_opts = rfl::json::read<rfl::Generic>(opts_json);

                commands::SaveDataParams params{
                        .data_key = _active_key,
                        .format = "csv",
                        .path = filepath,
                };
                if (format_opts) {
                    params.format_options = format_opts.value();
                }

                commands::CommandContext ctx;
                ctx.data_manager = dataManager();
                ctx.recorder = commandRecorder();

                auto const params_json = rfl::json::write(params);
                auto result = commands::executeSingleCommand("SaveData", params_json, ctx);

                if (result.success) {
                    QMessageBox::information(this, "Success", "Intervals saved successfully to CSV");
                } else {
                    QMessageBox::critical(this, "Error",
                                          QString("Failed to save: %1").arg(QString::fromStdString(result.error_message)));
                }
            });

    // Group filter signals
    connect(ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalIntervalSeriesInspector::_onGroupFilterChanged);
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
    TimeFrameIndex const converted_index = time_position.convertTo(series_timeframe);
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
        int64_t const start = std::min(_interval_start, _interval_end);
        int64_t const end = std::max(_interval_start, _interval_end);

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

    TimeFrameIndex const series_index(current_time);
    if (intervals->hasIntervalAtTime(series_index, *series_timeframe)) {
        intervals->setEventAtTime(series_index, false);
    } else {
        intervals->setEventAtTime(series_index, true);
    }
}

void DigitalIntervalSeriesInspector::_extendInterval() {
    std::vector<Interval> const selected_intervals = _getSelectedIntervals();
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

    for (Interval const & interval: selected_intervals) {
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
    QString const current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_interval_saver_widget);
    } else {
        // Handle other export types if added in the future
    }

    // Update filename based on new export type
    _updateFilename();
}

std::vector<Interval> DigitalIntervalSeriesInspector::_getSelectedIntervals() {
    if (_selection_provider) {
        return _selection_provider();
    }
    return {};
}

void DigitalIntervalSeriesInspector::_moveIntervalsToTarget(std::string const & target_key) {
    std::vector<Interval> const selected_intervals = _getSelectedIntervals();
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
    if (!_data_view) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalIntervalSeriesInspector: No intervals selected to copy." << std::endl;
        return;
    }

    auto source_interval_data = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    auto target_interval_data = dataManager()->getData<DigitalIntervalSeries>(target_key);

    if (!source_interval_data || !target_interval_data) {
        std::cerr << "DigitalIntervalSeriesInspector: Could not retrieve source or target data." << std::endl;
        return;
    }

    std::unordered_set<EntityId> const selected_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_copied = source_interval_data->copyByEntityIds(*target_interval_data, selected_set, NotifyObservers::Yes);

    if (total_copied > 0) {
        std::cout << "DigitalIntervalSeriesInspector: Copied " << total_copied
                  << " intervals from " << _active_key << " to " << target_key << std::endl;
    } else {
        std::cout << "DigitalIntervalSeriesInspector: No intervals found with the selected EntityIds to copy." << std::endl;
    }
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
    std::string const invalid_chars = "<>:\"/\\|?*";
    for (char const invalid_char: invalid_chars) {
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

    QString const current_export_type = ui->export_type_combo->currentText();
    std::string extension;

    if (current_export_type == "CSV") {
        extension = ".csv";
    }

    // Future export types can be added here
    // Example: if (current_export_type == "JSON") extension = ".json";

    return sanitized_key + extension;
}

void DigitalIntervalSeriesInspector::_updateFilename() {
    std::string const filename = _generateFilename();
    ui->filename_edit->setText(QString::fromStdString(filename));
}

void DigitalIntervalSeriesInspector::_deleteSelectedIntervals() {
    std::vector<Interval> const selected_intervals = _getSelectedIntervals();
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
    size_t const deleted_count = interval_data_ptr->removeIntervals(selected_intervals);

    if (deleted_count > 0) {
        std::cout << "DigitalIntervalSeriesInspector: Successfully deleted " << deleted_count
                  << " intervals." << std::endl;
        // The table will be automatically updated through the observer pattern
    } else {
        std::cout << "DigitalIntervalSeriesInspector: No intervals were deleted." << std::endl;
    }
}

void DigitalIntervalSeriesInspector::setGroupManager(GroupManager * group_manager) {
    BaseInspector::setGroupManager(group_manager);
    if (_data_view) {
        _data_view->setGroupManager(group_manager);
    }
    // Disconnect old connections if any
    if (groupManager()) {
        disconnect(groupManager(), nullptr, this, nullptr);
    }
    if (group_manager) {
        connectGroupManagerSignals(group_manager, this, &DigitalIntervalSeriesInspector::_onGroupChanged);
    }
    _populateGroupFilterCombo();
}

void DigitalIntervalSeriesInspector::_onGroupFilterChanged(int index) {
    if (!_data_view || !groupManager()) {
        return;
    }

    if (index == 0) {
        // "All Groups" selected
        _data_view->clearGroupFilter();
    } else {
        // Specific group selected (index - 1 because index 0 is "All Groups")
        auto groups = groupManager()->getGroups();
        auto group_ids = groups.keys();
        if (index - 1 < group_ids.size()) {
            int const group_id = group_ids[index - 1];
            _data_view->setGroupFilter(group_id);
        }
    }
}

void DigitalIntervalSeriesInspector::_onGroupChanged() {
    // Store current selection
    int const current_index = ui->groupFilterCombo->currentIndex();
    QString current_text;
    if (current_index >= 0 && current_index < ui->groupFilterCombo->count()) {
        current_text = ui->groupFilterCombo->itemText(current_index);
    }

    // Update the group filter combo box when groups change
    _populateGroupFilterCombo();

    // Restore selection
    restoreGroupFilterSelection(ui->groupFilterCombo, current_index, current_text);
}

void DigitalIntervalSeriesInspector::_populateGroupFilterCombo() {
    populateGroupFilterCombo(ui->groupFilterCombo, groupManager());
}

void DigitalIntervalSeriesInspector::_moveIntervalsToGroup(int group_id) {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalIntervalSeriesInspector: No intervals selected to move to group." << std::endl;
        return;
    }

    std::unordered_set<EntityId> const entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());

    // First, remove entities from their current groups
    groupManager()->ungroupEntities(entity_ids_set);

    // Then, assign entities to the specified group
    groupManager()->assignEntitiesToGroup(group_id, entity_ids_set);

    // Refresh the view to show updated group information
    if (_data_view) {
        _data_view->updateView();
    }

    std::cout << "DigitalIntervalSeriesInspector: Moved " << selected_entity_ids.size()
              << " selected intervals to group " << group_id << std::endl;
}

void DigitalIntervalSeriesInspector::_removeIntervalsFromGroup() {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalIntervalSeriesInspector: No intervals selected to remove from group." << std::endl;
        return;
    }

    std::unordered_set<EntityId> const entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());

    // Remove entities from all groups
    groupManager()->ungroupEntities(entity_ids_set);

    // Refresh the view to show updated group information
    if (_data_view) {
        _data_view->updateView();
    }

    std::cout << "DigitalIntervalSeriesInspector: Removed " << selected_entity_ids.size()
              << " selected intervals from their groups." << std::endl;
}
