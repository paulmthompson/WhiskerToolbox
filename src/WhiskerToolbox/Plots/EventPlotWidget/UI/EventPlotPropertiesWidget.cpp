#include "EventPlotPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/EventPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "UI/EventPlotWidget.hpp"

#include "ui_EventPlotPropertiesWidget.h"

#include <QColorDialog>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>

EventPlotPropertiesWidget::EventPlotPropertiesWidget(std::shared_ptr<EventPlotState> state,
                                                     std::shared_ptr<DataManager> data_manager,
                                                     QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _alignment_widget(nullptr),
      _plot_widget(nullptr),
      _range_controls(nullptr),
      _range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr),
      _dm_observer_id(-1) {
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

    // Set up background color display button
    ui->background_color_display_button->setFlat(false);
    ui->background_color_display_button->setEnabled(false);// Make it non-clickable, just for display

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
            this, &EventPlotPropertiesWidget::_onAddEventClicked);
    connect(ui->remove_event_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::_onRemoveEventClicked);
    connect(ui->plot_events_table, &QTableWidget::itemSelectionChanged,
            this, &EventPlotPropertiesWidget::_onPlotEventSelectionChanged);
    connect(ui->tick_thickness_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EventPlotPropertiesWidget::_onTickThicknessChanged);
    connect(ui->glyph_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventPlotPropertiesWidget::_onGlyphTypeChanged);
    connect(ui->color_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::_onColorButtonClicked);
    connect(ui->background_color_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::_onBackgroundColorButtonClicked);
    connect(ui->sorting_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventPlotPropertiesWidget::_onSortingModeChanged);

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
        connect(_state.get(), &EventPlotState::plotEventAdded,
                this, &EventPlotPropertiesWidget::_onStatePlotEventAdded);
        connect(_state.get(), &EventPlotState::plotEventRemoved,
                this, &EventPlotPropertiesWidget::_onStatePlotEventRemoved);
        connect(_state.get(), &EventPlotState::plotEventOptionsChanged,
                this, &EventPlotPropertiesWidget::_onStatePlotEventOptionsChanged);
        connect(_state.get(), &EventPlotState::backgroundColorChanged,
                this, &EventPlotPropertiesWidget::_onStateBackgroundColorChanged);
        connect(_state.get(), &EventPlotState::sortingModeChanged,
                this, &EventPlotPropertiesWidget::_onStateSortingModeChanged);

        // Initialize UI from state
        _updateUIFromState();
    }
}

void EventPlotPropertiesWidget::setPlotWidget(EventPlotWidget * plot_widget) {
    _plot_widget = plot_widget;

    if (!_plot_widget) {
        return;
    }

    // Get the relative time axis state from EventPlotState
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

    // Get the vertical axis state from the plot widget
    auto * vertical_axis_state = _plot_widget->getVerticalAxisState();

    if (vertical_axis_state) {
        // Create a collapsible section for the vertical axis range controls
        _vertical_range_controls_section = new Section(this, "Vertical Axis Range Controls");

        // Create new range controls that use the VerticalAxisState
        auto vertical_axis_with_controls = createVerticalAxisWithRangeControls(
            vertical_axis_state, nullptr, _vertical_range_controls_section);
        _vertical_range_controls = vertical_axis_with_controls.range_controls;

        // Set up the collapsible section (it starts collapsed by default)
        _vertical_range_controls_section->autoSetContentLayout();

        // Add the section to the main layout (after time axis range controls)
        int insert_index = _range_controls_section
                                   ? ui->main_layout->indexOf(_range_controls_section) + 1
                                   : ui->main_layout->indexOf(_alignment_widget) + 1;
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

EventPlotPropertiesWidget::~EventPlotPropertiesWidget() {
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void EventPlotPropertiesWidget::_populateAddEventComboBox() {
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

void EventPlotPropertiesWidget::_onAddEventClicked() {
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

void EventPlotPropertiesWidget::_onRemoveEventClicked() {
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

void EventPlotPropertiesWidget::_onPlotEventSelectionChanged() {
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
        ui->tick_thickness_spinbox->blockSignals(true);
        ui->tick_thickness_spinbox->setValue(2.0);
        ui->tick_thickness_spinbox->blockSignals(false);
        ui->glyph_type_combo->blockSignals(true);
        ui->glyph_type_combo->setCurrentIndex(0);
        ui->glyph_type_combo->blockSignals(false);
        _updateColorDisplay("#000000");
    }
}

void EventPlotPropertiesWidget::_updatePlotEventsTable() {
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

void EventPlotPropertiesWidget::_updateEventOptions(QString const & event_name) {
    if (!_state) {
        return;
    }

    auto options = _state->getPlotEventOptions(event_name);
    if (options) {
        // Update tick thickness
        ui->tick_thickness_spinbox->blockSignals(true);
        ui->tick_thickness_spinbox->setValue(options->tick_thickness);
        ui->tick_thickness_spinbox->blockSignals(false);

        // Update glyph type combo box
        int glyph_index = 0;// Default to Tick
        switch (options->glyph_type) {
            case EventGlyphType::Tick:
                glyph_index = 0;
                break;
            case EventGlyphType::Circle:
                glyph_index = 1;
                break;
            case EventGlyphType::Square:
                glyph_index = 2;
                break;
        }
        ui->glyph_type_combo->blockSignals(true);
        ui->glyph_type_combo->setCurrentIndex(glyph_index);
        ui->glyph_type_combo->blockSignals(false);

        // Update color display
        _updateColorDisplay(QString::fromStdString(options->hex_color));
    }
}

void EventPlotPropertiesWidget::_onStatePlotEventAdded(QString const & event_name) {
    Q_UNUSED(event_name)
    _updatePlotEventsTable();
}

void EventPlotPropertiesWidget::_onStatePlotEventRemoved(QString const & event_name) {
    Q_UNUSED(event_name)
    _updatePlotEventsTable();
    // Clear selection if the removed event was selected
    ui->plot_events_table->clearSelection();
    ui->remove_event_button->setEnabled(false);
    ui->event_options_widget->setEnabled(false);
}

void EventPlotPropertiesWidget::_onStatePlotEventOptionsChanged(QString const & event_name) {
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

void EventPlotPropertiesWidget::_updateUIFromState() {
    if (!_state) {
        return;
    }

    // Update alignment widget from state
    if (_alignment_widget) {
        _alignment_widget->updateUIFromState();
    }

    // Update plot events table
    _updatePlotEventsTable();

    // Update background color display
    _updateBackgroundColorDisplay(_state->getBackgroundColor());

    // Update sorting combo box
    ui->sorting_combo->blockSignals(true);
    ui->sorting_combo->setCurrentIndex(static_cast<int>(_state->getSortingMode()));
    ui->sorting_combo->blockSignals(false);
}

QString EventPlotPropertiesWidget::_getSelectedEventName() const {
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

void EventPlotPropertiesWidget::_updateColorDisplay(QString const & hex_color) {
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color));
}

void EventPlotPropertiesWidget::_updateBackgroundColorDisplay(QString const & hex_color) {
    // Update the background color display button with the new color
    ui->background_color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color));
}

void EventPlotPropertiesWidget::_onTickThicknessChanged(double value) {
    QString event_name = _getSelectedEventName();
    if (event_name.isEmpty() || !_state) {
        return;
    }

    auto options = _state->getPlotEventOptions(event_name);
    if (options) {
        options->tick_thickness = value;
        _state->updatePlotEventOptions(event_name, *options);
    }
}

void EventPlotPropertiesWidget::_onGlyphTypeChanged(int index) {
    QString event_name = _getSelectedEventName();
    if (event_name.isEmpty() || !_state) {
        return;
    }

    auto options = _state->getPlotEventOptions(event_name);
    if (options) {
        EventGlyphType glyph_type = EventGlyphType::Tick;// Default
        switch (index) {
            case 0:
                glyph_type = EventGlyphType::Tick;
                break;
            case 1:
                glyph_type = EventGlyphType::Circle;
                break;
            case 2:
                glyph_type = EventGlyphType::Square;
                break;
        }
        options->glyph_type = glyph_type;
        _state->updatePlotEventOptions(event_name, *options);
    }
}

void EventPlotPropertiesWidget::_onColorButtonClicked() {
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

void EventPlotPropertiesWidget::_onBackgroundColorButtonClicked() {
    if (!_state) {
        return;
    }

    // Get current background color
    QColor current_color = QColor(_state->getBackgroundColor());

    // Open color dialog
    QColor color = QColorDialog::getColor(current_color, this, "Choose Background Color");

    if (color.isValid()) {
        QString hex_color = color.name();
        _updateBackgroundColorDisplay(hex_color);
        _state->setBackgroundColor(hex_color);
    }
}

void EventPlotPropertiesWidget::_onStateBackgroundColorChanged(QString const & hex_color) {
    _updateBackgroundColorDisplay(hex_color);
}

void EventPlotPropertiesWidget::_onSortingModeChanged(int index) {
    if (!_state) {
        return;
    }

    TrialSortMode mode = TrialSortMode::TrialIndex;// Default
    switch (index) {
        case 0:
            mode = TrialSortMode::TrialIndex;
            break;
        case 1:
            mode = TrialSortMode::FirstEventLatency;
            break;
        case 2:
            mode = TrialSortMode::EventCount;
            break;
    }
    _state->setSortingMode(mode);
}

void EventPlotPropertiesWidget::_onStateSortingModeChanged(TrialSortMode mode) {
    ui->sorting_combo->blockSignals(true);
    ui->sorting_combo->setCurrentIndex(static_cast<int>(mode));
    ui->sorting_combo->blockSignals(false);
}
