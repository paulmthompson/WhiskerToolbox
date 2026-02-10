#include "PSTHPropertiesWidget.hpp"

#include "Core/PSTHState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "UI/PSTHWidget.hpp"

#include "ui_PSTHPropertiesWidget.h"

#include <QColorDialog>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>

PSTHPropertiesWidget::PSTHPropertiesWidget(std::shared_ptr<PSTHState> state,
                                            std::shared_ptr<DataManager> data_manager,
                                            QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PSTHPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _alignment_widget(nullptr),
      _plot_widget(nullptr),
      _range_controls(nullptr),
      _range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr),
      _dm_observer_id(-1)
{
    ui->setupUi(this);

    // Create and add PlotAlignmentWidget
    _alignment_widget = new PlotAlignmentWidget(_state->alignmentState(), _data_manager, this);
    // Replace the placeholder widget with the alignment widget
    int alignment_index = ui->main_layout->indexOf(ui->alignment_widget_placeholder);
    ui->main_layout->removeWidget(ui->alignment_widget_placeholder);
    ui->alignment_widget_placeholder->deleteLater();
    ui->main_layout->insertWidget(alignment_index, _alignment_widget);

    // Set up color display button
    ui->color_display_button->setFlat(false);
    ui->color_display_button->setEnabled(false);// Make it non-clickable, just for display

    // Set up plot events table
    ui->plot_events_table->setColumnCount(2);
    ui->plot_events_table->setHorizontalHeaderLabels(QStringList() << "Event Name" << "Data Key");
    ui->plot_events_table->horizontalHeader()->setStretchLastSection(true);
    ui->plot_events_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->plot_events_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->plot_events_table->verticalHeader()->setVisible(false);// Hide row numbers
    ui->plot_events_table->setShowGrid(true);

    // Connect UI signals to slots
    connect(ui->add_event_button, &QPushButton::clicked,
            this, &PSTHPropertiesWidget::_onAddEventClicked);
    connect(ui->remove_event_button, &QPushButton::clicked,
            this, &PSTHPropertiesWidget::_onRemoveEventClicked);
    connect(ui->plot_events_table, &QTableWidget::itemSelectionChanged,
            this, &PSTHPropertiesWidget::_onPlotEventSelectionChanged);
    connect(ui->color_button, &QPushButton::clicked,
            this, &PSTHPropertiesWidget::_onColorButtonClicked);
    connect(ui->style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PSTHPropertiesWidget::_onStyleChanged);
    connect(ui->bin_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PSTHPropertiesWidget::_onBinSizeChanged);

    // Populate combo boxes
    _populateAddEventComboBox();

    // Set up DataManager observer to refresh combo boxes when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateAddEventComboBox();
        });
    }

    // Connect state signals to update UI
    if (_state) {
        connect(_state.get(), &PSTHState::plotEventAdded,
                this, &PSTHPropertiesWidget::_onStatePlotEventAdded);
        connect(_state.get(), &PSTHState::plotEventRemoved,
                this, &PSTHPropertiesWidget::_onStatePlotEventRemoved);
        connect(_state.get(), &PSTHState::plotEventOptionsChanged,
                this, &PSTHPropertiesWidget::_onStatePlotEventOptionsChanged);
        connect(_state.get(), &PSTHState::styleChanged,
                this, [this](PSTHStyle style) {
                    ui->style_combo->blockSignals(true);
                    ui->style_combo->setCurrentIndex(style == PSTHStyle::Bar ? 0 : 1);
                    ui->style_combo->blockSignals(false);
                });
        connect(_state.get(), &PSTHState::binSizeChanged,
                this, [this](double bin_size) {
                    ui->bin_size_spinbox->blockSignals(true);
                    ui->bin_size_spinbox->setValue(bin_size);
                    ui->bin_size_spinbox->blockSignals(false);
                });
        // Note: yMin/yMax changes are handled by VerticalAxisRangeControls
        // No need to connect to yMinChanged/yMaxChanged here

        // Initialize UI from state
        _updateUIFromState();
    }
}

PSTHPropertiesWidget::~PSTHPropertiesWidget()
{
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void PSTHPropertiesWidget::setPlotWidget(PSTHWidget * plot_widget)
{
    _plot_widget = plot_widget;

    if (!_plot_widget) {
        return;
    }

    // Get the relative time axis state from PSTHState
    if (_state) {
        auto * time_axis_state = _state->relativeTimeAxisState();
        if (time_axis_state) {
            // Create a collapsible section for the range controls
            _range_controls_section = new Section(this, "Time Axis Range Controls");
            
            // Create new range controls that use the RelativeTimeAxisState
            _range_controls = new RelativeTimeAxisRangeControls(time_axis_state, _range_controls_section);
        
            // Set up the collapsible section (it starts collapsed by default)
            _range_controls_section->autoSetContentLayout();
            
            // Add the section to the main layout (after alignment widget)
            int insert_index = ui->main_layout->indexOf(_alignment_widget) + 1;
            ui->main_layout->insertWidget(insert_index, _range_controls_section);
        }
    }

    // Get the vertical axis state from PSTHState
    if (_state) {
        auto * vertical_axis_state = _state->verticalAxisState();
        if (vertical_axis_state) {
            // Create a collapsible section for the vertical axis range controls
            _vertical_range_controls_section = new Section(this, "Vertical Axis Range Controls");
            
            // Create new range controls that use the VerticalAxisState
            _vertical_range_controls = new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
            
            // Set up the collapsible section (it starts collapsed by default)
            _vertical_range_controls_section->autoSetContentLayout();
            
            // Add the section to the main layout (after time axis range controls)
            int insert_index = _range_controls_section 
                ? ui->main_layout->indexOf(_range_controls_section) + 1
                : ui->main_layout->indexOf(_alignment_widget) + 1;
            ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
        }
    }
}

void PSTHPropertiesWidget::_populateAddEventComboBox()
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
    for (auto const & key: event_keys) {
        ui->add_event_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }
}

void PSTHPropertiesWidget::_onAddEventClicked()
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

void PSTHPropertiesWidget::_onRemoveEventClicked()
{
    if (!_state) {
        return;
    }

    QList<QTableWidgetItem *> selected = ui->plot_events_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    int row = selected.first()->row();
    QTableWidgetItem * name_item = ui->plot_events_table->item(row, 0);
    if (name_item) {
        QString event_name = name_item->text();
        _state->removePlotEvent(event_name);
    }
}

void PSTHPropertiesWidget::_onPlotEventSelectionChanged()
{
    QList<QTableWidgetItem *> selected = ui->plot_events_table->selectedItems();
    bool has_selection = !selected.isEmpty();
    ui->remove_event_button->setEnabled(has_selection);
    ui->event_options_widget->setEnabled(has_selection);

    if (has_selection && _state) {
        int row = selected.first()->row();
        QTableWidgetItem * name_item = ui->plot_events_table->item(row, 0);
        if (name_item) {
            QString event_name = name_item->text();
            _updateEventOptions(event_name);
        }
    } else {
        // Clear options display - disable controls
        _updateColorDisplay("#000000");
    }
}

void PSTHPropertiesWidget::_updatePlotEventsTable()
{
    if (!_state) {
        return;
    }

    ui->plot_events_table->setRowCount(0);

    auto event_names = _state->getPlotEventNames();
    for (QString const & event_name: event_names) {
        auto options = _state->getPlotEventOptions(event_name);
        if (!options) {
            continue;
        }

        int row = ui->plot_events_table->rowCount();
        ui->plot_events_table->insertRow(row);

        QTableWidgetItem * name_item = new QTableWidgetItem(event_name);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_events_table->setItem(row, 0, name_item);

        QTableWidgetItem * key_item = new QTableWidgetItem(QString::fromStdString(options->event_key));
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

void PSTHPropertiesWidget::_updateEventOptions(QString const & event_name)
{
    if (!_state) {
        return;
    }

    auto options = _state->getPlotEventOptions(event_name);
    if (options) {
        // Update color display
        _updateColorDisplay(QString::fromStdString(options->hex_color));
    }
}

void PSTHPropertiesWidget::_onStatePlotEventAdded(QString const & event_name)
{
    Q_UNUSED(event_name)
    _updatePlotEventsTable();
}

void PSTHPropertiesWidget::_onStatePlotEventRemoved(QString const & event_name)
{
    Q_UNUSED(event_name)
    _updatePlotEventsTable();
    // Clear selection if the removed event was selected
    ui->plot_events_table->clearSelection();
    ui->remove_event_button->setEnabled(false);
    ui->event_options_widget->setEnabled(false);
}

void PSTHPropertiesWidget::_onStatePlotEventOptionsChanged(QString const & event_name)
{
    // Update the table and options if this event is selected
    _updatePlotEventsTable();
    QList<QTableWidgetItem *> selected = ui->plot_events_table->selectedItems();
    if (!selected.isEmpty()) {
        int row = selected.first()->row();
        QTableWidgetItem * name_item = ui->plot_events_table->item(row, 0);
        if (name_item && name_item->text() == event_name) {
            _updateEventOptions(event_name);
        }
    }
}

void PSTHPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    // Update alignment widget from state
    if (_alignment_widget) {
        _alignment_widget->updateUIFromState();
    }

    // Update global options
    ui->style_combo->blockSignals(true);
    ui->style_combo->setCurrentIndex(_state->getStyle() == PSTHStyle::Bar ? 0 : 1);
    ui->style_combo->blockSignals(false);

    ui->bin_size_spinbox->blockSignals(true);
    ui->bin_size_spinbox->setValue(_state->getBinSize());
    ui->bin_size_spinbox->blockSignals(false);

    // Note: yMin/yMax are handled by VerticalAxisRangeControls, not legacy spinboxes

    // Update plot events table
    _updatePlotEventsTable();
}

QString PSTHPropertiesWidget::_getSelectedEventName() const
{
    QList<QTableWidgetItem *> selected = ui->plot_events_table->selectedItems();
    if (selected.isEmpty()) {
        return QString();
    }

    int row = selected.first()->row();
    QTableWidgetItem * name_item = ui->plot_events_table->item(row, 0);
    if (name_item) {
        return name_item->text();
    }
    return QString();
}

void PSTHPropertiesWidget::_updateColorDisplay(QString const & hex_color)
{
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color));
}

void PSTHPropertiesWidget::_onColorButtonClicked()
{
    QString event_name = _getSelectedEventName();
    if (event_name.isEmpty() || !_state) {
        return;
    }

    // Get current color
    QColor current_color;
    auto options = _state->getPlotEventOptions(event_name);
    if (options) {
        current_color = QColor(QString::fromStdString(options->hex_color));
    } else {
        current_color = QColor("#000000");// Default black
    }

    // Open color dialog
    QColor color = QColorDialog::getColor(current_color, this, "Choose Color");

    if (color.isValid() && options) {
        QString hex_color = color.name();
        _updateColorDisplay(hex_color);
        options->hex_color = hex_color.toStdString();
        _state->updatePlotEventOptions(event_name, *options);
    }
}

void PSTHPropertiesWidget::_onStyleChanged(int index)
{
    if (!_state) {
        return;
    }

    PSTHStyle style = (index == 0) ? PSTHStyle::Bar : PSTHStyle::Line;
    _state->setStyle(style);
}

void PSTHPropertiesWidget::_onBinSizeChanged(double value)
{
    if (!_state) {
        return;
    }

    _state->setBinSize(value);
}
