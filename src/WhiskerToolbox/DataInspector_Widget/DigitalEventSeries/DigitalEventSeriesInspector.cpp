#include "DigitalEventSeriesInspector.hpp"
#include "ui_DigitalEventSeriesInspector.h"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "Commands/IO/SaveData.hpp"
#include "DataExport_Widget/DigitalTimeSeries/CSV/CSVEventSaver_Widget.hpp"
#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataInspector_Widget/Inspectors/GroupFilterHelper.hpp"
#include "DataManager.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/IO/SaveData.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalEventSeriesDataView.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

#include <rfl/json.hpp>

#include <filesystem>
#include <iostream>
#include <unordered_set>

DigitalEventSeriesInspector::DigitalEventSeriesInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager,
        QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent),
      ui(new Ui::DigitalEventSeriesInspector) {
    ui->setupUi(this);

    _connectSignals();

    // Set up export section
    ui->export_section->setTitle("Export");
    ui->export_section->autoSetContentLayout();

    // Initialize group filter combo box
    _populateGroupFilterCombo();

    // Set up group manager signals if group manager exists
    connectGroupManagerSignals(groupManager(), this, &DigitalEventSeriesInspector::_onGroupChanged);
}

DigitalEventSeriesInspector::~DigitalEventSeriesInspector() {
    removeCallbacks();
    delete ui;
}

void DigitalEventSeriesInspector::setActiveKey(std::string const & key) {
    _active_key = key;

    if (!_active_key.empty() && _callback_id != -1) {
        dataManager()->removeCallbackFromData(_active_key, _callback_id);
    }

    _assignCallbacks();

    _calculateEvents();

    // Update filename based on new active key
    _updateFilename();
}

void DigitalEventSeriesInspector::removeCallbacks() {
    if (!_active_key.empty() && _callback_id != -1) {
        dataManager()->removeCallbackFromData(_active_key, _callback_id);
        _callback_id = -1;
    }
}

void DigitalEventSeriesInspector::updateView() {
    // DigitalEventSeriesInspector auto-updates through callbacks
    // No explicit updateTable method
}

void DigitalEventSeriesInspector::setDataView(DigitalEventSeriesDataView * view) {
    // Disconnect from old view if any
    if (_data_view) {
        disconnect(_data_view, &DigitalEventSeriesDataView::moveEventsRequested, this, nullptr);
        disconnect(_data_view, &DigitalEventSeriesDataView::copyEventsRequested, this, nullptr);
        disconnect(_data_view, &DigitalEventSeriesDataView::moveEventsToGroupRequested, this, nullptr);
        disconnect(_data_view, &DigitalEventSeriesDataView::removeEventsFromGroupRequested, this, nullptr);
        disconnect(_data_view, &DigitalEventSeriesDataView::deleteEventsRequested, this, nullptr);
    }

    _data_view = view;
    if (_data_view) {
        // Set group manager on the view
        if (groupManager()) {
            _data_view->setGroupManager(groupManager());
        }

        // Connect to view signals for move/copy operations
        connect(_data_view, &DigitalEventSeriesDataView::moveEventsRequested,
                this, &DigitalEventSeriesInspector::_moveEventsToTarget);
        connect(_data_view, &DigitalEventSeriesDataView::copyEventsRequested,
                this, &DigitalEventSeriesInspector::_copyEventsToTarget);

        // Connect to view signals for group management operations
        connect(_data_view, &DigitalEventSeriesDataView::moveEventsToGroupRequested,
                this, &DigitalEventSeriesInspector::_moveEventsToGroup);
        connect(_data_view, &DigitalEventSeriesDataView::removeEventsFromGroupRequested,
                this, &DigitalEventSeriesInspector::_removeEventsFromGroup);

        // Connect to view signal for delete operation
        connect(_data_view, &DigitalEventSeriesDataView::deleteEventsRequested,
                this, &DigitalEventSeriesInspector::_deleteSelectedEvents);
    }
}

void DigitalEventSeriesInspector::setGroupManager(GroupManager * group_manager) {
    BaseInspector::setGroupManager(group_manager);
    if (_data_view) {
        _data_view->setGroupManager(group_manager);
    }
    // Disconnect old connections if any
    if (groupManager()) {
        disconnect(groupManager(), nullptr, this, nullptr);
    }
    if (group_manager) {
        connectGroupManagerSignals(group_manager, this, &DigitalEventSeriesInspector::_onGroupChanged);
    }
    _populateGroupFilterCombo();
}

void DigitalEventSeriesInspector::_connectSignals() {
    connect(ui->add_event_button, &QPushButton::clicked, this, &DigitalEventSeriesInspector::_addEventButton);
    connect(ui->remove_event_button, &QPushButton::clicked, this, &DigitalEventSeriesInspector::_removeEventButton);

    // Export section connections
    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalEventSeriesInspector::_onExportTypeChanged);
    connect(ui->csv_event_saver_widget, &CSVEventSaver_Widget::saveEventCSVRequested,
            this, [this](CSVEventSaverOptions options) {
                auto output_path = dataManager()->getOutputPath();
                if (output_path.empty()) {
                    QMessageBox::warning(this, "Warning",
                                         "Please set an output directory in the Data Manager settings");
                    return;
                }

                auto filename = ui->filename_edit->text().toStdString();
                auto const filepath = (std::filesystem::path(output_path) / filename).string();

                // Populate path fields so the registry saver has them
                options.parent_dir = output_path;
                options.filename = filename;

                // Serialize the CSV options to rfl::Generic for format_options
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
                    QMessageBox::information(this, "Success", "Events saved successfully to CSV");
                } else {
                    QMessageBox::critical(this, "Error",
                                          QString("Failed to save: %1").arg(QString::fromStdString(result.error_message)));
                }
            });

    // Group filter signals
    connect(ui->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalEventSeriesInspector::_onGroupFilterChanged);
}

void DigitalEventSeriesInspector::_assignCallbacks() {
    if (!_active_key.empty()) {
        _callback_id = dataManager()->addCallbackToData(_active_key, [this]() {
            _calculateEvents();
        });
    }
}

void DigitalEventSeriesInspector::_calculateEvents() {
    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);
    if (!events) return;

    ui->total_events_label->setText(QString::number(events->size()));
}

int64_t DigitalEventSeriesInspector::_getCurrentTimeInSeriesFrame() const {
    if (!state()) {
        return -1;
    }

    auto const & time_position = state()->current_position;
    if (!time_position.isValid() || !time_position.time_frame) {
        return -1;
    }

    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);
    if (!events) {
        return -1;
    }

    auto series_timeframe = events->getTimeFrame();
    if (!series_timeframe) {
        return -1;
    }

    // Convert the state's time_position to the series' timeframe
    TimeFrameIndex const converted_index = time_position.convertTo(series_timeframe);
    return converted_index.getValue();
}

void DigitalEventSeriesInspector::_addEventButton() {
    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalEventSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    events->addEvent(TimeFrameIndex(current_time));

    std::cout << "Number of events is " << events->size() << std::endl;

    _calculateEvents();
}

void DigitalEventSeriesInspector::_removeEventButton() {
    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalEventSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    bool const removed = events->removeEvent(TimeFrameIndex(current_time));
    static_cast<void>(removed);

    _calculateEvents();
}

void DigitalEventSeriesInspector::_onExportTypeChanged(int index) {
    // Update the stacked widget to show the correct saver options widget
    ui->stacked_saver_options->setCurrentIndex(index);

    // Update filename when export type changes
    _updateFilename();
}

std::string DigitalEventSeriesInspector::_generateFilename() const {
    if (_active_key.empty()) {
        return "events.csv";
    }

    // Get the export type
    QString const export_type = ui->export_type_combo->currentText();

    if (export_type == "CSV") {
        return _active_key + ".csv";
    }

    return _active_key + ".csv";// Default to CSV
}

void DigitalEventSeriesInspector::_updateFilename() {
    ui->filename_edit->setText(QString::fromStdString(_generateFilename()));
}

void DigitalEventSeriesInspector::_moveEventsToTarget(std::string const & target_key) {
    if (!_data_view) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalEventSeriesInspector: No events selected to move." << std::endl;
        return;
    }

    auto source_event_data = dataManager()->getData<DigitalEventSeries>(_active_key);
    auto target_event_data = dataManager()->getData<DigitalEventSeries>(target_key);

    if (!source_event_data || !target_event_data) {
        std::cerr << "DigitalEventSeriesInspector: Could not retrieve source or target data." << std::endl;
        return;
    }

    std::unordered_set<EntityId> const selected_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_moved = source_event_data->moveByEntityIds(*target_event_data, selected_set, NotifyObservers::Yes);

    if (total_moved > 0) {
        std::cout << "DigitalEventSeriesInspector: Moved " << total_moved
                  << " events from " << _active_key << " to " << target_key << std::endl;
    } else {
        std::cout << "DigitalEventSeriesInspector: No events found with the selected EntityIds to move." << std::endl;
    }
}

void DigitalEventSeriesInspector::_copyEventsToTarget(std::string const & target_key) {
    if (!_data_view) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalEventSeriesInspector: No events selected to copy." << std::endl;
        return;
    }

    auto source_event_data = dataManager()->getData<DigitalEventSeries>(_active_key);
    auto target_event_data = dataManager()->getData<DigitalEventSeries>(target_key);

    if (!source_event_data || !target_event_data) {
        std::cerr << "DigitalEventSeriesInspector: Could not retrieve source or target data." << std::endl;
        return;
    }

    std::unordered_set<EntityId> const selected_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_copied = source_event_data->copyByEntityIds(*target_event_data, selected_set, NotifyObservers::Yes);

    if (total_copied > 0) {
        std::cout << "DigitalEventSeriesInspector: Copied " << total_copied
                  << " events from " << _active_key << " to " << target_key << std::endl;
    } else {
        std::cout << "DigitalEventSeriesInspector: No events found with the selected EntityIds to copy." << std::endl;
    }
}

void DigitalEventSeriesInspector::_deleteSelectedEvents() {
    if (!_data_view) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalEventSeriesInspector: No events selected to delete." << std::endl;
        return;
    }

    auto event_data = dataManager()->getData<DigitalEventSeries>(_active_key);
    if (!event_data) {
        std::cerr << "DigitalEventSeriesInspector: DigitalEventSeries object ('" << _active_key << "') not found." << std::endl;
        return;
    }

    std::unordered_set<EntityId> const selected_set(selected_entity_ids.begin(), selected_entity_ids.end());
    std::size_t const total_deleted = event_data->deleteByEntityIds(selected_set, NotifyObservers::Yes);

    std::cout << "DigitalEventSeriesInspector: Deleted " << total_deleted
              << " events from '" << _active_key << "'." << std::endl;
}

void DigitalEventSeriesInspector::_moveEventsToGroup(int group_id) {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalEventSeriesInspector: No events selected to move to group." << std::endl;
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

    std::cout << "DigitalEventSeriesInspector: Moved " << selected_entity_ids.size()
              << " selected events to group " << group_id << std::endl;
}

void DigitalEventSeriesInspector::_removeEventsFromGroup() {
    if (!_data_view || !groupManager()) {
        return;
    }

    auto selected_entity_ids = _data_view->getSelectedEntityIds();
    if (selected_entity_ids.empty()) {
        std::cout << "DigitalEventSeriesInspector: No events selected to remove from group." << std::endl;
        return;
    }

    std::unordered_set<EntityId> const entity_ids_set(selected_entity_ids.begin(), selected_entity_ids.end());

    // Remove entities from all groups
    groupManager()->ungroupEntities(entity_ids_set);

    // Refresh the view to show updated group information
    if (_data_view) {
        _data_view->updateView();
    }

    std::cout << "DigitalEventSeriesInspector: Removed " << selected_entity_ids.size()
              << " selected events from their groups." << std::endl;
}

void DigitalEventSeriesInspector::_onGroupFilterChanged(int index) {
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

void DigitalEventSeriesInspector::_onGroupChanged() {
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

void DigitalEventSeriesInspector::_populateGroupFilterCombo() {
    populateGroupFilterCombo(ui->groupFilterCombo, groupManager());
}
