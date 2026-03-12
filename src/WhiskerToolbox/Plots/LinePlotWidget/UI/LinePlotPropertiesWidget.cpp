#include "LinePlotPropertiesWidget.hpp"

#include "Core/LinePlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Plots/Common/LineStyleControls/LineStyleControls.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/SelectionInstructions.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "UI/LinePlotWidget.hpp"

#include "ui_LinePlotPropertiesWidget.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

LinePlotPropertiesWidget::LinePlotPropertiesWidget(std::shared_ptr<LinePlotState> state,
                                                  std::shared_ptr<DataManager> data_manager,
                                                  QWidget * parent)
    : QWidget(parent),
      ui(new Ui::LinePlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _alignment_widget(nullptr),
      _plot_widget(nullptr),
      _range_controls(nullptr),
      _range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr),
      _line_style_controls(nullptr),
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

    // Hide old bespoke style controls (replaced by LineStyleControls)
    ui->line_thickness_label->hide();
    ui->line_thickness_spinbox->hide();
    ui->color_label->hide();
    ui->color_widget->hide();

    // Create LineStyleControls (initially unbound — bound when series is selected)
    _line_style_controls = new LineStyleControls(nullptr, this);
    _line_style_controls->setEnabled(false);
    // Insert after series_options_label
    ui->series_options_layout->insertWidget(1, _line_style_controls);

    // Set up plot series table
    ui->plot_series_table->setColumnCount(2);
    ui->plot_series_table->setHorizontalHeaderLabels(QStringList() << "Series Name" << "Data Key");
    ui->plot_series_table->horizontalHeader()->setStretchLastSection(true);
    ui->plot_series_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->plot_series_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->plot_series_table->verticalHeader()->setVisible(false);// Hide row numbers
    ui->plot_series_table->setShowGrid(true);

    // Connect UI signals to slots
    connect(ui->add_series_button, &QPushButton::clicked,
            this, &LinePlotPropertiesWidget::_onAddSeriesClicked);
    connect(ui->remove_series_button, &QPushButton::clicked,
            this, &LinePlotPropertiesWidget::_onRemoveSeriesClicked);
    connect(ui->plot_series_table, &QTableWidget::itemSelectionChanged,
            this, &LinePlotPropertiesWidget::_onPlotSeriesSelectionChanged);

    // Populate combo boxes
    _populateAddSeriesComboBox();

    // Set up DataManager observer to refresh combo boxes when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateAddSeriesComboBox();
        });
    }

    // Connect state signals to update UI
    if (_state) {
        connect(_state.get(), &LinePlotState::plotSeriesAdded,
                this, &LinePlotPropertiesWidget::_onStatePlotSeriesAdded);
        connect(_state.get(), &LinePlotState::plotSeriesRemoved,
                this, &LinePlotPropertiesWidget::_onStatePlotSeriesRemoved);
        connect(_state.get(), &LinePlotState::plotSeriesOptionsChanged,
                this, &LinePlotPropertiesWidget::_onStatePlotSeriesOptionsChanged);

        // Initialize UI from state
        _updateUIFromState();
    }

    // === Selection Section ===
    {
        auto * selection_section = new Section(this, "Selection");

        auto * sel_layout = new QVBoxLayout();
        sel_layout->setContentsMargins(4, 4, 4, 4);
        sel_layout->setSpacing(4);

        auto * instructions_label = new QLabel();
        instructions_label->setWordWrap(true);
        instructions_label->setStyleSheet("color: #888; font-size: 11px;");
        instructions_label->setText(WhiskerToolbox::Plots::SelectionInstructions::lineCrossing());
        sel_layout->addWidget(instructions_label);

        // Color by group checkbox
        _color_by_group_checkbox = new QCheckBox("Color by group assignment");
        _color_by_group_checkbox->setToolTip(
            "When enabled, lines are colored according to their group color.\n"
            "Lines not in any group use the default line color above.");
        if (_state) {
            _color_by_group_checkbox->setChecked(_state->colorByGroup());
        }
        sel_layout->addWidget(_color_by_group_checkbox);

        selection_section->setContentLayout(*sel_layout);

        // Insert at the end (before stretcher)
        ui->main_layout->addWidget(selection_section);

        connect(_color_by_group_checkbox, &QCheckBox::toggled,
                this, &LinePlotPropertiesWidget::_onColorByGroupToggled);
    }
}

LinePlotPropertiesWidget::~LinePlotPropertiesWidget()
{
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void LinePlotPropertiesWidget::setPlotWidget(LinePlotWidget * plot_widget)
{
    _plot_widget = plot_widget;

    if (!_plot_widget) {
        return;
    }

    if (_state) {
        auto * time_axis_state = _state->relativeTimeAxisState();
        if (time_axis_state) {
            _range_controls_section = new Section(this, "Time Axis Range Controls");
            _range_controls = new RelativeTimeAxisRangeControls(time_axis_state, _range_controls_section);
            _range_controls_section->autoSetContentLayout();
            int insert_index = ui->main_layout->indexOf(_alignment_widget) + 1;
            ui->main_layout->insertWidget(insert_index, _range_controls_section);
        }
    }

    if (_state) {
        auto * vertical_axis_state = _state->verticalAxisState();
        if (vertical_axis_state) {
            _vertical_range_controls_section = new Section(this, "Vertical Axis Range Controls");
            _vertical_range_controls = new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
            _vertical_range_controls_section->autoSetContentLayout();
            int insert_index = _range_controls_section
                    ? ui->main_layout->indexOf(_range_controls_section) + 1
                    : ui->main_layout->indexOf(_alignment_widget) + 1;
            ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
        }
    }
}

void LinePlotPropertiesWidget::_populateAddSeriesComboBox()
{
    ui->add_series_combo->clear();

    if (!_data_manager) {
        return;
    }

    // Get all AnalogTimeSeries keys only
    auto series_keys = _data_manager->getKeys<AnalogTimeSeries>();

    if (series_keys.empty()) {
        return;
    }

    // Sort keys before adding to combo box
    std::sort(series_keys.begin(), series_keys.end());

    // Add AnalogTimeSeries keys
    for (auto const & key: series_keys) {
        ui->add_series_combo->addItem(QString::fromStdString(key), QString::fromStdString(key));
    }
}

void LinePlotPropertiesWidget::_onAddSeriesClicked()
{
    if (!_state || !_data_manager) {
        return;
    }

    QString series_key = ui->add_series_combo->currentData().toString();
    if (series_key.isEmpty()) {
        return;
    }

    // Use the series key as the name (could be made more sophisticated)
    QString series_name = series_key;
    _state->addPlotSeries(series_name, series_key);
}

void LinePlotPropertiesWidget::_onRemoveSeriesClicked()
{
    if (!_state) {
        return;
    }

    QList<QTableWidgetItem *> selected = ui->plot_series_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    int row = selected.first()->row();
    QTableWidgetItem * name_item = ui->plot_series_table->item(row, 0);
    if (name_item) {
        QString series_name = name_item->text();
        _state->removePlotSeries(series_name);
    }
}

void LinePlotPropertiesWidget::_onPlotSeriesSelectionChanged()
{
    QList<QTableWidgetItem *> selected = ui->plot_series_table->selectedItems();
    bool has_selection = !selected.isEmpty();
    ui->remove_series_button->setEnabled(has_selection);
    ui->series_options_widget->setEnabled(has_selection);
    _line_style_controls->setEnabled(has_selection);

    if (has_selection && _state) {
        int row = selected.first()->row();
        QTableWidgetItem * name_item = ui->plot_series_table->item(row, 0);
        if (name_item) {
            QString series_name = name_item->text();
            _updateLineStyleControlsBinding(series_name);
        }
    } else {
        // Unbind line style controls when no selection
        _line_style_controls->setLineStyleState(nullptr);
    }
}

void LinePlotPropertiesWidget::_updatePlotSeriesTable()
{
    if (!_state) {
        return;
    }

    ui->plot_series_table->setRowCount(0);

    auto series_names = _state->getPlotSeriesNames();
    for (QString const & series_name: series_names) {
        auto options = _state->getPlotSeriesOptions(series_name);
        if (!options) {
            continue;
        }

        int row = ui->plot_series_table->rowCount();
        ui->plot_series_table->insertRow(row);

        QTableWidgetItem * name_item = new QTableWidgetItem(series_name);
        name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_series_table->setItem(row, 0, name_item);

        QTableWidgetItem * key_item = new QTableWidgetItem(QString::fromStdString(options->series_key));
        key_item->setFlags(key_item->flags() & ~Qt::ItemIsEditable);
        ui->plot_series_table->setItem(row, 1, key_item);
    }

    // Resize table to fit content dynamically
    ui->plot_series_table->resizeRowsToContents();

    int row_count = ui->plot_series_table->rowCount();
    if (row_count == 0) {
        // If no rows, set minimum height to just show header
        ui->plot_series_table->setMinimumHeight(ui->plot_series_table->horizontalHeader()->height());
        ui->plot_series_table->setMaximumHeight(ui->plot_series_table->horizontalHeader()->height());
    } else {
        // Calculate height: header + (row height * row count)
        int header_height = ui->plot_series_table->horizontalHeader()->height();
        int row_height = ui->plot_series_table->rowHeight(0);
        int total_height = header_height + (row_height * row_count);

        ui->plot_series_table->setMinimumHeight(total_height);
        ui->plot_series_table->setMaximumHeight(total_height);
    }
}

void LinePlotPropertiesWidget::_updateLineStyleControlsBinding(QString const & series_name)
{
    if (!_state || !_line_style_controls) {
        return;
    }

    auto * style_state = _state->lineStyleStateForSeries(series_name);
    _line_style_controls->setLineStyleState(style_state);
}

void LinePlotPropertiesWidget::_onStatePlotSeriesAdded(QString const & series_name)
{
    Q_UNUSED(series_name)
    _updatePlotSeriesTable();
}

void LinePlotPropertiesWidget::_onStatePlotSeriesRemoved(QString const & series_name)
{
    Q_UNUSED(series_name)
    _updatePlotSeriesTable();
    // Clear selection if the removed series was selected
    ui->plot_series_table->clearSelection();
    ui->remove_series_button->setEnabled(false);
    ui->series_options_widget->setEnabled(false);
    _line_style_controls->setLineStyleState(nullptr);
    _line_style_controls->setEnabled(false);
}

void LinePlotPropertiesWidget::_onStatePlotSeriesOptionsChanged(QString const & series_name)
{
    // Update the table; LineStyleControls auto-updates from its bound state
    Q_UNUSED(series_name)
    _updatePlotSeriesTable();
}

void LinePlotPropertiesWidget::_onColorByGroupToggled(bool checked)
{
    if (_state) {
        _state->setColorByGroup(checked);
    }
}

void LinePlotPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    // Update alignment widget from state
    if (_alignment_widget) {
        _alignment_widget->updateUIFromState();
    }

    // Update plot series table
    _updatePlotSeriesTable();
}

QString LinePlotPropertiesWidget::_getSelectedSeriesName() const
{
    QList<QTableWidgetItem *> selected = ui->plot_series_table->selectedItems();
    if (selected.isEmpty()) {
        return QString();
    }

    int row = selected.first()->row();
    QTableWidgetItem * name_item = ui->plot_series_table->item(row, 0);
    if (name_item) {
        return name_item->text();
    }
    return QString();
}
