#include "PlotAlignmentWidget.hpp"

#include "Core/PlotAlignmentState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "ui_PlotAlignmentWidget.h"

#include <algorithm>

PlotAlignmentWidget::PlotAlignmentWidget(PlotAlignmentState * state,
                                         std::shared_ptr<DataManager> data_manager,
                                         QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PlotAlignmentWidget),
      _state(state),
      _data_manager(data_manager),
      _dm_observer_id(-1)
{
    ui->setupUi(this);

    // Set up interval alignment combo box
    ui->interval_alignment_combo->setCurrentIndex(0);// Default to Beginning

    // Connect UI signals to slots
    connect(ui->alignment_event_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlotAlignmentWidget::_onAlignmentEventChanged);
    connect(ui->interval_alignment_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlotAlignmentWidget::_onIntervalAlignmentChanged);
    connect(ui->offset_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlotAlignmentWidget::_onOffsetChanged);
    connect(ui->window_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlotAlignmentWidget::_onWindowSizeChanged);

    // Populate combo boxes
    _populateAlignmentEventComboBox();

    // Set up DataManager observer to refresh combo boxes when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateAlignmentEventComboBox();
            _updateEventCount();
        });
    }

    // Connect state signals to update UI
    if (_state) {
        connect(_state, &PlotAlignmentState::alignmentEventKeyChanged,
                this, &PlotAlignmentWidget::_onStateAlignmentEventKeyChanged);
        connect(_state, &PlotAlignmentState::intervalAlignmentTypeChanged,
                this, &PlotAlignmentWidget::_onStateIntervalAlignmentTypeChanged);
        connect(_state, &PlotAlignmentState::offsetChanged,
                this, &PlotAlignmentWidget::_onStateOffsetChanged);
        connect(_state, &PlotAlignmentState::windowSizeChanged,
                this, &PlotAlignmentWidget::_onStateWindowSizeChanged);

        // Initialize UI from state
        updateUIFromState();
    }
}

PlotAlignmentWidget::~PlotAlignmentWidget()
{
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void PlotAlignmentWidget::_populateAlignmentEventComboBox()
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
    for (auto const & key: event_keys) {
        ui->alignment_event_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }

    // Add DigitalIntervalSeries keys
    for (auto const & key: interval_keys) {
        ui->alignment_event_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }
}

void PlotAlignmentWidget::_updateEventCount()
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

void PlotAlignmentWidget::_onAlignmentEventChanged(int index)
{
    Q_UNUSED(index)
    QString key = ui->alignment_event_combo->currentData().toString();
    if (_state) {
        _state->setAlignmentEventKey(key);
    }
    _updateEventCount();
}

void PlotAlignmentWidget::_onIntervalAlignmentChanged(int index)
{
    if (!_state) {
        return;
    }

    IntervalAlignmentType type = (index == 0) ? IntervalAlignmentType::Beginning : IntervalAlignmentType::End;
    _state->setIntervalAlignmentType(type);
}

void PlotAlignmentWidget::_onOffsetChanged(double value)
{
    if (_state) {
        _state->setOffset(value);
    }
}

void PlotAlignmentWidget::_onWindowSizeChanged(double value)
{
    if (_state) {
        _state->setWindowSize(value);
    }
}

void PlotAlignmentWidget::_onStateAlignmentEventKeyChanged(QString const & key)
{
    // Update combo box without triggering signal
    int index = ui->alignment_event_combo->findData(key);
    if (index >= 0) {
        ui->alignment_event_combo->blockSignals(true);
        ui->alignment_event_combo->setCurrentIndex(index);
        ui->alignment_event_combo->blockSignals(false);
    } else {
        ui->alignment_event_combo->blockSignals(true);
        ui->alignment_event_combo->setCurrentIndex(0);// None
        ui->alignment_event_combo->blockSignals(false);
    }
    _updateEventCount();
}

void PlotAlignmentWidget::_onStateIntervalAlignmentTypeChanged(IntervalAlignmentType type)
{
    int index = (type == IntervalAlignmentType::Beginning) ? 0 : 1;
    ui->interval_alignment_combo->blockSignals(true);
    ui->interval_alignment_combo->setCurrentIndex(index);
    ui->interval_alignment_combo->blockSignals(false);
}

void PlotAlignmentWidget::_onStateOffsetChanged(double offset)
{
    ui->offset_spinbox->blockSignals(true);
    ui->offset_spinbox->setValue(offset);
    ui->offset_spinbox->blockSignals(false);
}

void PlotAlignmentWidget::_onStateWindowSizeChanged(double window_size)
{
    ui->window_size_spinbox->blockSignals(true);
    ui->window_size_spinbox->setValue(window_size);
    ui->window_size_spinbox->blockSignals(false);
}

void PlotAlignmentWidget::updateUIFromState()
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
        ui->alignment_event_combo->setCurrentIndex(0);// None
    }

    // Update interval alignment combo box
    IntervalAlignmentType type = _state->getIntervalAlignmentType();
    int alignment_index = (type == IntervalAlignmentType::Beginning) ? 0 : 1;
    ui->interval_alignment_combo->setCurrentIndex(alignment_index);

    // Update offset spinbox
    ui->offset_spinbox->setValue(_state->getOffset());

    // Update window size spinbox
    ui->window_size_spinbox->setValue(_state->getWindowSize());

    // Update event count
    _updateEventCount();
}
