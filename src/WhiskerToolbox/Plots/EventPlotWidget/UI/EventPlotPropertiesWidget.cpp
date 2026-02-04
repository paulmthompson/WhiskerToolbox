#include "EventPlotPropertiesWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "ui_EventPlotPropertiesWidget.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>

#include <algorithm>

EventPlotPropertiesWidget::EventPlotPropertiesWidget(std::shared_ptr<EventPlotState> state,
                                                     std::shared_ptr<DataManager> data_manager,
                                                     QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _dm_observer_id(-1)
{
    ui->setupUi(this);

    // Set up interval alignment combo box
    ui->interval_alignment_combo->setCurrentIndex(0);  // Default to Beginning

    // Set up plot events table
    ui->plot_events_table->setColumnCount(2);
    ui->plot_events_table->setHorizontalHeaderLabels(QStringList() << "Event Name" << "Data Key");
    ui->plot_events_table->horizontalHeader()->setStretchLastSection(true);
    ui->plot_events_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->plot_events_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->plot_events_table->verticalHeader()->setVisible(false);  // Hide row numbers
    ui->plot_events_table->setShowGrid(true);

    // Connect UI signals to slots
    connect(ui->alignment_event_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventPlotPropertiesWidget::_onAlignmentEventChanged);
    connect(ui->interval_alignment_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventPlotPropertiesWidget::_onIntervalAlignmentChanged);
    connect(ui->offset_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EventPlotPropertiesWidget::_onOffsetChanged);
    connect(ui->window_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EventPlotPropertiesWidget::_onWindowSizeChanged);
    connect(ui->add_event_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::_onAddEventClicked);
    connect(ui->remove_event_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::_onRemoveEventClicked);
    connect(ui->plot_events_table, &QTableWidget::itemSelectionChanged,
            this, &EventPlotPropertiesWidget::_onPlotEventSelectionChanged);

    // Populate combo boxes
    _populateAddEventComboBox();
    _populateAlignmentEventComboBox();

    // Set up DataManager observer to refresh combo boxes when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateAddEventComboBox();
            _populateAlignmentEventComboBox();
            _updateEventCount();
        });
    }

    // Connect state signals to update UI
    if (_state) {
        connect(_state.get(), &EventPlotState::alignmentEventKeyChanged,
                this, &EventPlotPropertiesWidget::_onStateAlignmentEventKeyChanged);
        connect(_state.get(), &EventPlotState::intervalAlignmentTypeChanged,
                this, &EventPlotPropertiesWidget::_onStateIntervalAlignmentTypeChanged);
        connect(_state.get(), &EventPlotState::offsetChanged,
                this, &EventPlotPropertiesWidget::_onStateOffsetChanged);
        connect(_state.get(), &EventPlotState::windowSizeChanged,
                this, &EventPlotPropertiesWidget::_onStateWindowSizeChanged);
        connect(_state.get(), &EventPlotState::plotEventAdded,
                this, &EventPlotPropertiesWidget::_onStatePlotEventAdded);
        connect(_state.get(), &EventPlotState::plotEventRemoved,
                this, &EventPlotPropertiesWidget::_onStatePlotEventRemoved);
        connect(_state.get(), &EventPlotState::plotEventOptionsChanged,
                this, &EventPlotPropertiesWidget::_onStatePlotEventOptionsChanged);

        // Initialize UI from state
        _updateUIFromState();
    }
}

EventPlotPropertiesWidget::~EventPlotPropertiesWidget()
{
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void EventPlotPropertiesWidget::_populateAddEventComboBox()
{
    ui->add_event_combo->clear();

    if (!_data_manager) {
        return;
    }

    // Get all DigitalEventSeries keys only
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();

    if (event_keys.empty()) {
        return;
    }

    // Sort keys before adding to combo box
    std::sort(event_keys.begin(), event_keys.end());

    // Add DigitalEventSeries keys
    for (auto const & key : event_keys) {
        ui->add_event_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }
}

void EventPlotPropertiesWidget::_populateAlignmentEventComboBox()
{
    ui->alignment_event_combo->clear();
    ui->alignment_event_combo->addItem("(None)", QString());

    if (!_data_manager) {
        return;
    }

    // Get all DigitalEventSeries keys
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();

    // Get all DigitalIntervalSeries keys
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();

    if (event_keys.empty() && interval_keys.empty()) {
        return;
    }

    // Sort keys before adding to combo box
    std::sort(event_keys.begin(), event_keys.end());
    std::sort(interval_keys.begin(), interval_keys.end());

    // Add DigitalEventSeries keys
    for (auto const & key : event_keys) {
        ui->alignment_event_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }

    // Add DigitalIntervalSeries keys
    for (auto const & key : interval_keys) {
        ui->alignment_event_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }
}

void EventPlotPropertiesWidget::_updateEventCount()
{
    if (!_data_manager) {
        ui->total_count_label->setText("Total: 0");
        ui->interval_alignment_widget->setVisible(false);
        return;
    }

    QString current_key = ui->alignment_event_combo->currentData().toString();
    if (current_key.isEmpty()) {
        ui->total_count_label->setText("Total: 0");
        ui->interval_alignment_widget->setVisible(false);
        return;
    }

    DM_DataType data_type = _data_manager->getType(current_key.toStdString());

    if (data_type == DM_DataType::DigitalEvent) {
        auto series = _data_manager->getData<DigitalEventSeries>(current_key.toStdString());
        if (series) {
            size_t count = series->size();
            ui->total_count_label->setText(QString("Total: %1 events").arg(count));
        } else {
            ui->total_count_label->setText("Total: 0 events");
        }
        ui->interval_alignment_widget->setVisible(false);
    } else if (data_type == DM_DataType::DigitalInterval) {
        auto series = _data_manager->getData<DigitalIntervalSeries>(current_key.toStdString());
        if (series) {
            size_t count = series->size();
            ui->total_count_label->setText(QString("Total: %1 intervals").arg(count));
        } else {
            ui->total_count_label->setText("Total: 0 intervals");
        }
        ui->interval_alignment_widget->setVisible(true);
    } else {
        ui->total_count_label->setText("Total: 0");
        ui->interval_alignment_widget->setVisible(false);
    }
}

void EventPlotPropertiesWidget::_onAddEventClicked()
{
    if (!_state || !_data_manager) {
        return;
    }

    QString event_key = ui->add_event_combo->currentData().toString();
    if (event_key.isEmpty()) {
        return;
    }

    // Use the event key as the name (could be made more sophisticated)
    QString event_name = event_key;
    _state->addPlotEvent(event_name, event_key);
}

void EventPlotPropertiesWidget::_onRemoveEventClicked()
{
    if (!_state) {
        return;
    }

    QList<QTableWidgetItem*> selected = ui->plot_events_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    int row = selected.first()->row();
    QTableWidgetItem* name_item = ui->plot_events_table->item(row, 0);
    if (name_item) {
        QString event_name = name_item->text();
        _state->removePlotEvent(event_name);
    }
}

void EventPlotPropertiesWidget::_onPlotEventSelectionChanged()
{
    QList<QTableWidgetItem*> selected = ui->plot_events_table->selectedItems();
    bool has_selection = !selected.isEmpty();
    ui->remove_event_button->setEnabled(has_selection);
    ui->event_options_widget->setEnabled(has_selection);

    if (has_selection && _state) {
        int row = selected.first()->row();
        QTableWidgetItem* name_item = ui->plot_events_table->item(row, 0);
        if (name_item) {
            QString event_name = name_item->text();
            _updateEventOptions(event_name);
        }
    } else {
        // Clear options display
        ui->event_options_placeholder->setText("(No options available yet)");
    }
}

void EventPlotPropertiesWidget::_updatePlotEventsTable()
{
    if (!_state) {
        return;
    }

    ui->plot_events_table->setRowCount(0);

    auto event_names = _state->getPlotEventNames();
    for (QString const & event_name : event_names) {
        auto options = _state->getPlotEventOptions(event_name);
        if (!options) {
            continue;
        }

        int row = ui->plot_events_table->rowCount();
        ui->plot_events_table->insertRow(row);

        QTableWidgetItem* name_item = new QTableWidgetItem(event_name);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_events_table->setItem(row, 0, name_item);

        QTableWidgetItem* key_item = new QTableWidgetItem(QString::fromStdString(options->event_key));
        key_item->setFlags(key_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_events_table->setItem(row, 1, key_item);
    }

    // Resize table to fit content dynamically
    ui->plot_events_table->resizeRowsToContents();
    
    int row_count = ui->plot_events_table->rowCount();
    if (row_count == 0) {
        // If no rows, set minimum height to just show header
        ui->plot_events_table->setMinimumHeight(ui->plot_events_table->horizontalHeader()->height());
        ui->plot_events_table->setMaximumHeight(ui->plot_events_table->horizontalHeader()->height());
    } else {
        // Calculate height: header + (row height * row count)
        int header_height = ui->plot_events_table->horizontalHeader()->height();
        int row_height = ui->plot_events_table->rowHeight(0);
        int total_height = header_height + (row_height * row_count);
        
        ui->plot_events_table->setMinimumHeight(total_height);
        ui->plot_events_table->setMaximumHeight(total_height);
    }
}

void EventPlotPropertiesWidget::_updateEventOptions(QString const & event_name)
{
    if (!_state) {
        return;
    }

    auto options = _state->getPlotEventOptions(event_name);
    if (options) {
        // For now, just show a placeholder. Options can be added here later.
        ui->event_options_placeholder->setText(
            QString("Options for: %1\nData Key: %2")
                .arg(event_name)
                .arg(QString::fromStdString(options->event_key)));
    }
}

void EventPlotPropertiesWidget::_onAlignmentEventChanged(int index)
{
    Q_UNUSED(index)
    QString key = ui->alignment_event_combo->currentData().toString();
    if (_state) {
        _state->setAlignmentEventKey(key);
    }
    _updateEventCount();
}

void EventPlotPropertiesWidget::_onIntervalAlignmentChanged(int index)
{
    if (!_state) {
        return;
    }

    IntervalAlignmentType type = (index == 0) ? IntervalAlignmentType::Beginning : IntervalAlignmentType::End;
    _state->setIntervalAlignmentType(type);
}

void EventPlotPropertiesWidget::_onOffsetChanged(double value)
{
    if (_state) {
        _state->setOffset(value);
    }
}

void EventPlotPropertiesWidget::_onWindowSizeChanged(double value)
{
    if (_state) {
        _state->setWindowSize(value);
    }
}

void EventPlotPropertiesWidget::_onStatePlotEventAdded(QString const & event_name)
{
    Q_UNUSED(event_name)
    _updatePlotEventsTable();
}

void EventPlotPropertiesWidget::_onStatePlotEventRemoved(QString const & event_name)
{
    Q_UNUSED(event_name)
    _updatePlotEventsTable();
    // Clear selection if the removed event was selected
    ui->plot_events_table->clearSelection();
    ui->remove_event_button->setEnabled(false);
    ui->event_options_widget->setEnabled(false);
}

void EventPlotPropertiesWidget::_onStatePlotEventOptionsChanged(QString const & event_name)
{
    // Update the table and options if this event is selected
    _updatePlotEventsTable();
    QList<QTableWidgetItem*> selected = ui->plot_events_table->selectedItems();
    if (!selected.isEmpty()) {
        int row = selected.first()->row();
        QTableWidgetItem* name_item = ui->plot_events_table->item(row, 0);
        if (name_item && name_item->text() == event_name) {
            _updateEventOptions(event_name);
        }
    }
}

void EventPlotPropertiesWidget::_onStateAlignmentEventKeyChanged(QString const & key)
{
    // Update combo box without triggering signal
    int index = ui->alignment_event_combo->findData(key);
    if (index >= 0) {
        ui->alignment_event_combo->blockSignals(true);
        ui->alignment_event_combo->setCurrentIndex(index);
        ui->alignment_event_combo->blockSignals(false);
    } else {
        ui->alignment_event_combo->blockSignals(true);
        ui->alignment_event_combo->setCurrentIndex(0);  // None
        ui->alignment_event_combo->blockSignals(false);
    }
    _updateEventCount();
}

void EventPlotPropertiesWidget::_onStateIntervalAlignmentTypeChanged(IntervalAlignmentType type)
{
    int index = (type == IntervalAlignmentType::Beginning) ? 0 : 1;
    ui->interval_alignment_combo->blockSignals(true);
    ui->interval_alignment_combo->setCurrentIndex(index);
    ui->interval_alignment_combo->blockSignals(false);
}

void EventPlotPropertiesWidget::_onStateOffsetChanged(double offset)
{
    ui->offset_spinbox->blockSignals(true);
    ui->offset_spinbox->setValue(offset);
    ui->offset_spinbox->blockSignals(false);
}

void EventPlotPropertiesWidget::_onStateWindowSizeChanged(double window_size)
{
    ui->window_size_spinbox->blockSignals(true);
    ui->window_size_spinbox->setValue(window_size);
    ui->window_size_spinbox->blockSignals(false);
}

void EventPlotPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    // Update alignment event combo box
    QString key = _state->getAlignmentEventKey();
    int index = ui->alignment_event_combo->findData(key);
    if (index >= 0) {
        ui->alignment_event_combo->setCurrentIndex(index);
    } else {
        ui->alignment_event_combo->setCurrentIndex(0);  // None
    }

    // Update interval alignment combo box
    IntervalAlignmentType type = _state->getIntervalAlignmentType();
    int alignment_index = (type == IntervalAlignmentType::Beginning) ? 0 : 1;
    ui->interval_alignment_combo->setCurrentIndex(alignment_index);

    // Update offset spinbox
    ui->offset_spinbox->setValue(_state->getOffset());

    // Update window size spinbox
    ui->window_size_spinbox->setValue(_state->getWindowSize());

    // Update plot events table
    _updatePlotEventsTable();

    // Update event count
    _updateEventCount();
}
