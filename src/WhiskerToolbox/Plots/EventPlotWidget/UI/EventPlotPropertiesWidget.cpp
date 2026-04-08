#include "EventPlotPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/EventPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Plots/Common/GlyphStyleWidget/GlyphStyleControls.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "UI/EventPlotWidget.hpp"

#include "ui_EventPlotPropertiesWidget.h"

#include <QColorDialog>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

EventPlotPropertiesWidget::EventPlotPropertiesWidget(std::shared_ptr<EventPlotState> state,
                                                     std::shared_ptr<DataManager> data_manager,
                                                     QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _state(std::move(std::move(state))),
      _data_manager(std::move(std::move(data_manager))),
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
    int const alignment_index = ui->main_layout->indexOf(ui->alignment_widget_placeholder);
    ui->main_layout->removeWidget(ui->alignment_widget_placeholder);
    ui->alignment_widget_placeholder->deleteLater();
    ui->main_layout->insertWidget(alignment_index, _alignment_widget);

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
    connect(ui->background_color_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::_onBackgroundColorButtonClicked);
    connect(ui->sorting_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EventPlotPropertiesWidget::_onSortingModeChanged);
    connect(ui->export_svg_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::exportSVGRequested);
    connect(ui->export_csv_button, &QPushButton::clicked,
            this, &EventPlotPropertiesWidget::exportCSVRequested);

    // === Glyph Style Options collapsible section ===
    // Inserted after the add event widget. The GlyphStyleControls are
    // bound to the per-key state of the selected row.
    _glyph_style_section = new Section(this, "Glyph Options");
    if (_state) {
        _glyph_style_controls = new GlyphStyleControls(nullptr, this);
        _glyph_style_controls->setEnabled(false);

        auto * glyph_layout = new QVBoxLayout();
        glyph_layout->setContentsMargins(4, 4, 4, 4);
        glyph_layout->addWidget(_glyph_style_controls);
        _glyph_style_section->setContentLayout(*glyph_layout);
    }
    // Insert after the add event widget
    {
        int const insert_idx = ui->main_layout->indexOf(ui->add_event_widget) + 1;
        ui->main_layout->insertWidget(insert_idx, _glyph_style_section);
    }

    // === Edge Selector for interval series ===
    // Create edge selector widget (hidden by default, shown when an interval key is selected)
    {
        _edge_selector_widget = new QWidget(this);
        auto * edge_layout = new QHBoxLayout(_edge_selector_widget);
        edge_layout->setContentsMargins(0, 0, 0, 0);
        edge_layout->setSpacing(4);

        auto * edge_label = new QLabel(QStringLiteral("Edge:"), _edge_selector_widget);
        _edge_selector_combo = new QComboBox(_edge_selector_widget);
        _edge_selector_combo->addItem(QStringLiteral("Start"), static_cast<int>(IntervalAlignmentType::Beginning));
        _edge_selector_combo->addItem(QStringLiteral("End"), static_cast<int>(IntervalAlignmentType::End));

        edge_layout->addWidget(edge_label);
        edge_layout->addWidget(_edge_selector_combo, 1);

        // Insert between the add_event_widget combo row and the glyph section
        int const insert_idx = ui->main_layout->indexOf(ui->add_event_widget) + 1;
        ui->main_layout->insertWidget(insert_idx, _edge_selector_widget);
        _edge_selector_widget->setVisible(false);

        // Show/hide edge selector when the add_event_combo selection changes
        connect(ui->add_event_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EventPlotPropertiesWidget::_onAddEventComboChanged);
    }

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
            int const insert_index = ui->main_layout->indexOf(_alignment_widget) + 1;
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
        int const insert_index = _range_controls_section
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

    // Get DigitalEventSeries keys
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();

    // Sort keys before adding to combo box
    std::sort(event_keys.begin(), event_keys.end());

    // Add DigitalEventSeries keys
    for (auto const & key: event_keys) {
        ui->add_event_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }

    // Get DigitalIntervalSeries keys
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    std::sort(interval_keys.begin(), interval_keys.end());

    // Add DigitalIntervalSeries keys (prefixed to distinguish in the combo)
    for (auto const & key: interval_keys) {
        ui->add_event_combo->addItem(
                QString::fromStdString(key) + QStringLiteral(" [interval]"),
                QString::fromStdString(key));
    }
}

void EventPlotPropertiesWidget::_onAddEventClicked() {
    if (!_state || !_data_manager) {
        return;
    }

    QString const event_key = ui->add_event_combo->currentData().toString();
    if (event_key.isEmpty()) {
        return;
    }

    // Determine if this is an interval series and get the edge type
    std::optional<IntervalAlignmentType> interval_edge;
    DM_DataType const type = _data_manager->getType(event_key.toStdString());
    if (type == DM_DataType::DigitalInterval) {
        int edge_val = _edge_selector_combo->currentData().toInt();
        interval_edge = static_cast<IntervalAlignmentType>(edge_val);
    }

    // Build a descriptive name: for intervals, include edge info
    QString event_name = event_key;
    if (interval_edge.has_value()) {
        event_name += (*interval_edge == IntervalAlignmentType::Beginning)
                              ? QStringLiteral(" [start]")
                              : QStringLiteral(" [end]");
    }

    _state->addPlotEvent(event_name, event_key, interval_edge);
}

void EventPlotPropertiesWidget::_onRemoveEventClicked() {
    if (!_state) {
        return;
    }

    QList<QTableWidgetItem *> selected = ui->plot_events_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    int const row = selected.first()->row();
    QTableWidgetItem * name_item = ui->plot_events_table->item(row, 0);
    if (name_item) {
        QString const event_name = name_item->text();
        // Detach glyph controls from the state BEFORE it gets freed
        // by removePlotEvent(). Otherwise setGlyphStyleState(nullptr)
        // in the signal handler will dereference a dangling pointer.
        if (_glyph_style_controls) {
            _glyph_style_controls->setGlyphStyleState(nullptr);
        }
        _state->removePlotEvent(event_name);
    }
}

void EventPlotPropertiesWidget::_onAddEventComboChanged(int /*index*/) {
    if (!_data_manager || !_edge_selector_widget) {
        return;
    }

    QString const event_key = ui->add_event_combo->currentData().toString();
    if (event_key.isEmpty()) {
        _edge_selector_widget->setVisible(false);
        return;
    }

    DM_DataType const type = _data_manager->getType(event_key.toStdString());
    _edge_selector_widget->setVisible(type == DM_DataType::DigitalInterval);
}

void EventPlotPropertiesWidget::_onPlotEventSelectionChanged() {
    QList<QTableWidgetItem *> const selected = ui->plot_events_table->selectedItems();
    bool const has_selection = !selected.isEmpty();
    ui->remove_event_button->setEnabled(has_selection);
    _updateGlyphStyleControls();
}

void EventPlotPropertiesWidget::_updatePlotEventsTable() {
    if (!_state) {
        return;
    }

    // Block signals while rebuilding the table to prevent selectionChanged
    // from firing mid-update and accessing stale/freed GlyphStyleState pointers.
    QSignalBlocker const blocker(ui->plot_events_table);

    ui->plot_events_table->setRowCount(0);

    auto event_names = _state->getPlotEventNames();
    for (QString const & event_name: event_names) {
        auto options = _state->getPlotEventOptions(event_name);
        if (!options) {
            continue;
        }

        int const row = ui->plot_events_table->rowCount();
        ui->plot_events_table->insertRow(row);

        auto * name_item = new QTableWidgetItem(event_name);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_events_table->setItem(row, 0, name_item);

        auto * key_item = new QTableWidgetItem(QString::fromStdString(options->event_key));
        key_item->setFlags(key_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_events_table->setItem(row, 1, key_item);
    }

    // Resize table to fit content dynamically
    ui->plot_events_table->resizeRowsToContents();

    int const row_count = ui->plot_events_table->rowCount();
    if (row_count == 0) {
        // If no rows, set minimum height to just show header
        ui->plot_events_table->setMinimumHeight(ui->plot_events_table->horizontalHeader()->height());
        ui->plot_events_table->setMaximumHeight(ui->plot_events_table->horizontalHeader()->height());
    } else {
        // Calculate height: header + (row height * row count)
        int const header_height = ui->plot_events_table->horizontalHeader()->height();
        int const row_height = ui->plot_events_table->rowHeight(0);
        int const total_height = header_height + (row_height * row_count);

        ui->plot_events_table->setMinimumHeight(total_height);
        ui->plot_events_table->setMaximumHeight(total_height);
    }
}

void EventPlotPropertiesWidget::_onStatePlotEventAdded(QString const & event_name) {
    Q_UNUSED(event_name)
    _updatePlotEventsTable();
    _updateGlyphStyleControls();
}

void EventPlotPropertiesWidget::_onStatePlotEventRemoved(QString const & event_name) {
    Q_UNUSED(event_name)
    // Clear glyph controls FIRST — the removed event's GlyphStyleState is
    // already freed by the time this slot runs, so we must detach from it
    // before any UI update can re-read the dangling pointer.
    if (_glyph_style_controls) {
        _glyph_style_controls->setGlyphStyleState(nullptr);
    }
    _updatePlotEventsTable();
    ui->plot_events_table->clearSelection();
    ui->remove_event_button->setEnabled(false);
}

void EventPlotPropertiesWidget::_onStatePlotEventOptionsChanged(QString const & event_name) {
    Q_UNUSED(event_name)
    // Update the table (event key may have changed)
    _updatePlotEventsTable();
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

void EventPlotPropertiesWidget::_updateGlyphStyleControls() {
    if (!_glyph_style_controls || !_state) {
        return;
    }

    QList<QTableWidgetItem *> const selected = ui->plot_events_table->selectedItems();
    if (selected.isEmpty()) {
        _glyph_style_controls->setGlyphStyleState(nullptr);
        return;
    }

    int const row = selected.first()->row();
    QTableWidgetItem const * item = ui->plot_events_table->item(row, 0);
    if (!item) {
        _glyph_style_controls->setGlyphStyleState(nullptr);
        return;
    }

    GlyphStyleState * glyph_state = _state->glyphStyleStateForKey(item->text());
    _glyph_style_controls->setGlyphStyleState(glyph_state);
}

void EventPlotPropertiesWidget::_updateBackgroundColorDisplay(QString const & hex_color) {
    // Update the background color display button with the new color
    ui->background_color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color));
}

void EventPlotPropertiesWidget::_onBackgroundColorButtonClicked() {
    if (!_state) {
        return;
    }

    // Get current background color
    QColor const current_color = QColor(_state->getBackgroundColor());

    // Open color dialog
    QColor const color = QColorDialog::getColor(current_color, this, "Choose Background Color");

    if (color.isValid()) {
        QString const hex_color = color.name();
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
