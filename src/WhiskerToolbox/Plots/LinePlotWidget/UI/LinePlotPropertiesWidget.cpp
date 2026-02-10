#include "LinePlotPropertiesWidget.hpp"

#include "Core/LinePlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "UI/LinePlotWidget.hpp"

#include "ui_LinePlotPropertiesWidget.h"

#include <QColorDialog>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

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
    connect(ui->line_thickness_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LinePlotPropertiesWidget::_onLineThicknessChanged);
    connect(ui->color_button, &QPushButton::clicked,
            this, &LinePlotPropertiesWidget::_onColorButtonClicked);

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

    if (has_selection && _state) {
        int row = selected.first()->row();
        QTableWidgetItem * name_item = ui->plot_series_table->item(row, 0);
        if (name_item) {
            QString series_name = name_item->text();
            _updateSeriesOptions(series_name);
        }
    } else {
        // Clear options display - disable controls
        ui->line_thickness_spinbox->blockSignals(true);
        ui->line_thickness_spinbox->setValue(1.0);
        ui->line_thickness_spinbox->blockSignals(false);
        _updateColorDisplay("#000000");
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

void LinePlotPropertiesWidget::_updateSeriesOptions(QString const & series_name)
{
    if (!_state) {
        return;
    }

    auto options = _state->getPlotSeriesOptions(series_name);
    if (options) {
        // Update line thickness
        ui->line_thickness_spinbox->blockSignals(true);
        ui->line_thickness_spinbox->setValue(options->line_thickness);
        ui->line_thickness_spinbox->blockSignals(false);

        // Update color display
        _updateColorDisplay(QString::fromStdString(options->hex_color));
    }
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
}

void LinePlotPropertiesWidget::_onStatePlotSeriesOptionsChanged(QString const & series_name)
{
    // Update the table and options if this series is selected
    _updatePlotSeriesTable();
    QList<QTableWidgetItem *> selected = ui->plot_series_table->selectedItems();
    if (!selected.isEmpty()) {
        int row = selected.first()->row();
        QTableWidgetItem * name_item = ui->plot_series_table->item(row, 0);
        if (name_item && name_item->text() == series_name) {
            _updateSeriesOptions(series_name);
        }
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

void LinePlotPropertiesWidget::_updateColorDisplay(QString const & hex_color)
{
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color));
}

void LinePlotPropertiesWidget::_onLineThicknessChanged(double value)
{
    QString series_name = _getSelectedSeriesName();
    if (series_name.isEmpty() || !_state) {
        return;
    }

    auto options = _state->getPlotSeriesOptions(series_name);
    if (options) {
        options->line_thickness = value;
        _state->updatePlotSeriesOptions(series_name, *options);
    }
}

void LinePlotPropertiesWidget::_onColorButtonClicked()
{
    QString series_name = _getSelectedSeriesName();
    if (series_name.isEmpty() || !_state) {
        return;
    }

    // Get current color
    QColor current_color;
    auto options = _state->getPlotSeriesOptions(series_name);
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
        _state->updatePlotSeriesOptions(series_name, *options);
    }
}
