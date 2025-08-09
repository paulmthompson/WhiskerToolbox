#include "EventPlotPropertiesWidget.hpp"
#include "EventPlotOpenGLWidget.hpp"
#include "EventPlotWidget.hpp"


#include "DataManager/utils/TableView/TableRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/core/TableView.h"
#include "DataManager/DataManagerTypes.hpp"
#include "ui_EventPlotPropertiesWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>

#include <memory>
#include <optional>
#include <algorithm>
#include <numeric>
#include <cmath>

#include "Analysis_Dashboard/DataView/Transforms/SortByColumnTransform.hpp"
#include "Analysis_Dashboard/DataView/Transforms/FilterByRangeTransform.hpp"
#include "Analysis_Dashboard/DataView/Transforms/ColorByFeatureTransform.hpp"

EventPlotPropertiesWidget::EventPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _event_plot_widget(nullptr),
      _applying_properties(false) {
    ui->setupUi(this);
    setupConnections();
    setupYAxisFeatureTable();
}

EventPlotPropertiesWidget::~EventPlotPropertiesWidget() {
    delete ui;
}

void EventPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    updateAvailableTables();
    updateAvailableDataSources();
    updateAvailableSortColumns();
}

void EventPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    _plot_widget = plot_widget;
    _event_plot_widget = qobject_cast<EventPlotWidget *>(plot_widget);

    if (_event_plot_widget) {
        updateFromPlot();

        // Connect to pan offset changes to update view bounds labels
        if (_event_plot_widget->getOpenGLWidget()) {
            connect(_event_plot_widget->getOpenGLWidget(), &EventPlotOpenGLWidget::panOffsetChanged,
                    this, &EventPlotPropertiesWidget::onViewBoundsChanged);
        }
    }
}

void EventPlotPropertiesWidget::updateFromPlot() {
    if (!_event_plot_widget) {
        return;
    }

    // Update UI with current plot settings
    QStringList current_keys = _event_plot_widget->getEventDataKeys();
    if (!current_keys.isEmpty()) {
        setSelectedXAxisDataSource(current_keys.first());
    }

    // Update interval settings visibility
    updateIntervalSettingsVisibility();

    // Update capture range from current X-axis range
    int negative_range, positive_range;
    _event_plot_widget->getXAxisRange(negative_range, positive_range);
    if (negative_range == positive_range) {
        setCaptureRange(negative_range);
    }

    // Update view bounds labels
    updateViewBoundsLabels();

    // Update zoom level
    if (_event_plot_widget->getOpenGLWidget()) {
        float current_zoom = _event_plot_widget->getOpenGLWidget()->getZoomLevel();
        ui->zoom_level_spinbox->blockSignals(true);
        ui->zoom_level_spinbox->setValue(static_cast<double>(current_zoom));
        ui->zoom_level_spinbox->blockSignals(false);

        bool tooltips_enabled = _event_plot_widget->getOpenGLWidget()->getTooltipsEnabled();
        ui->tooltips_checkbox->blockSignals(true);
        ui->tooltips_checkbox->setChecked(tooltips_enabled);
        ui->tooltips_checkbox->blockSignals(false);

        // Update dark mode checkbox state
        auto plot_theme = _event_plot_widget->getOpenGLWidget()->getPlotTheme();
        ui->dark_mode_checkbox->blockSignals(true);
        ui->dark_mode_checkbox->setChecked(plot_theme == EventPlotOpenGLWidget::PlotTheme::Dark);
        ui->dark_mode_checkbox->blockSignals(false);
    }
}

void EventPlotPropertiesWidget::applyToPlot() {
    if (!_event_plot_widget) {
        return;
    }

    // Set flag to prevent signal emission during property application
    _applying_properties = true;
    
    // Apply current settings to the plot widget
    updatePlotWidget();
    
    // Reset flag
    _applying_properties = false;
}

void EventPlotPropertiesWidget::updateAvailableDataSources() {
    if (!_data_manager || !ui->x_axis_combo) {
        return;
    }

    ui->x_axis_combo->clear();
    ui->x_axis_combo->addItem("Select a data source...", "");

    // Get all available keys from DataManager
    std::vector<std::string> all_keys = _data_manager->getAllKeys();

    for (std::string const & key: all_keys) {
        DM_DataType data_type = _data_manager->getType(key);

        // Only include DigitalEventSeries and DigitalIntervalSeries
        if (data_type == DM_DataType::DigitalEvent || data_type == DM_DataType::DigitalInterval) {
            QString display_text = QString::fromStdString(key);
            if (data_type == DM_DataType::DigitalEvent) {
                display_text += " (Events)";
            } else {
                display_text += " (Intervals)";
            }

            ui->x_axis_combo->addItem(display_text, QString::fromStdString(key));
        }
    }

    // Update info label based on selection
    updateXAxisInfoLabel();
}

void EventPlotPropertiesWidget::onXAxisDataSourceChanged() {
    updateXAxisInfoLabel();
    updateIntervalSettingsVisibility();
    updatePlotWidget();
}

void EventPlotPropertiesWidget::onIntervalSettingChanged() {
    updatePlotWidget();
}

void EventPlotPropertiesWidget::onYAxisFeatureSelected(QString const & feature) {
    // Feature selection is handled by the feature table widget itself
    Q_UNUSED(feature)
}

void EventPlotPropertiesWidget::onYAxisFeatureAdded(QString const & feature) {
    _selected_y_axis_features.insert(feature);

    updatePlotWidget();
}

void EventPlotPropertiesWidget::onYAxisFeatureRemoved(QString const & feature) {
    _selected_y_axis_features.erase(feature);
    updatePlotWidget();
}

void EventPlotPropertiesWidget::onZoomLevelChanged(double value) {
    if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
        _event_plot_widget->getOpenGLWidget()->setZoomLevel(static_cast<float>(value));
    }
}

void EventPlotPropertiesWidget::onResetViewClicked() {
    if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
        _event_plot_widget->getOpenGLWidget()->setZoomLevel(1.0f);
        _event_plot_widget->getOpenGLWidget()->setPanOffset(0.0f, 0.0f);
    }
}

void EventPlotPropertiesWidget::onTooltipsEnabledChanged(bool enabled) {
    if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
        _event_plot_widget->getOpenGLWidget()->setTooltipsEnabled(enabled);
    }
}

void EventPlotPropertiesWidget::onDarkModeToggled(bool enabled) {
    if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
        auto plotTheme = enabled ? EventPlotOpenGLWidget::PlotTheme::Dark : EventPlotOpenGLWidget::PlotTheme::Light;
        _event_plot_widget->getOpenGLWidget()->setPlotTheme(plotTheme);
    }
}

void EventPlotPropertiesWidget::setupConnections() {
    // Connect table selection signals
    if (ui->table_combo) {
        connect(ui->table_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EventPlotPropertiesWidget::onTableSelectionChanged);
    }

    if (ui->column_combo) {
        connect(ui->column_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EventPlotPropertiesWidget::onColumnSelectionChanged);
    }

    // Connect UI signals to slots
    if (ui->x_axis_combo) {
        connect(ui->x_axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EventPlotPropertiesWidget::onXAxisDataSourceChanged);
    }

    if (ui->interval_beginning_radio) {
        connect(ui->interval_beginning_radio, &QRadioButton::toggled,
                this, &EventPlotPropertiesWidget::onIntervalSettingChanged);
    }

    if (ui->interval_end_radio) {
        connect(ui->interval_end_radio, &QRadioButton::toggled,
                this, &EventPlotPropertiesWidget::onIntervalSettingChanged);
    }

    if (ui->zoom_level_spinbox) {
        connect(ui->zoom_level_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &EventPlotPropertiesWidget::onZoomLevelChanged);
    }

    if (ui->reset_view_button) {
        connect(ui->reset_view_button, &QPushButton::clicked,
                this, &EventPlotPropertiesWidget::onResetViewClicked);
    }

    if (ui->tooltips_checkbox) {
        connect(ui->tooltips_checkbox, &QCheckBox::toggled,
                this, &EventPlotPropertiesWidget::onTooltipsEnabledChanged);
    }

    if (ui->dark_mode_checkbox) {
        connect(ui->dark_mode_checkbox, &QCheckBox::toggled,
                this, &EventPlotPropertiesWidget::onDarkModeToggled);
    }

    if (ui->capture_range_spinbox) {
        connect(ui->capture_range_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &EventPlotPropertiesWidget::onCaptureRangeChanged);
    }

    // Sorting controls
    if (ui->sorting_enabled_checkbox) {
        connect(ui->sorting_enabled_checkbox, &QCheckBox::toggled,
                this, &EventPlotPropertiesWidget::onSortingToggled);
    }
    if (ui->sort_primary_combo) {
        connect(ui->sort_primary_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EventPlotPropertiesWidget::onSortingChanged);
    }
    if (ui->sort_secondary_combo) {
        connect(ui->sort_secondary_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EventPlotPropertiesWidget::onSortingChanged);
    }
    if (ui->sort_order_combo) {
        connect(ui->sort_order_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EventPlotPropertiesWidget::onSortingChanged);
        ui->sort_order_combo->setCurrentIndex(0); // Ascending default
    }

    // Filter controls
    if (ui->filter_enabled_checkbox) {
        connect(ui->filter_enabled_checkbox, &QCheckBox::toggled, this, &EventPlotPropertiesWidget::onFilterToggled);
    }
    if (ui->filter_column_combo) {
        connect(ui->filter_column_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EventPlotPropertiesWidget::onFilterChanged);
    }
    if (ui->filter_comparator_combo) {
        connect(ui->filter_comparator_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EventPlotPropertiesWidget::onFilterChanged);
    }
    if (ui->filter_value_spin) {
        connect(ui->filter_value_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EventPlotPropertiesWidget::onFilterChanged);
    }

    // Color controls
    if (ui->color_enabled_checkbox) {
        connect(ui->color_enabled_checkbox, &QCheckBox::toggled, this, &EventPlotPropertiesWidget::onColorToggled);
    }
    if (ui->color_column_combo) {
        connect(ui->color_column_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EventPlotPropertiesWidget::onColorChanged);
    }
    if (ui->color_mode_combo) {
        connect(ui->color_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EventPlotPropertiesWidget::onColorChanged);
    }
    if (ui->color_false_spin) {
        connect(ui->color_false_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &EventPlotPropertiesWidget::onColorChanged);
    }
    if (ui->color_true_spin) {
        connect(ui->color_true_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &EventPlotPropertiesWidget::onColorChanged);
    }
    if (ui->color_min_spin) {
        connect(ui->color_min_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EventPlotPropertiesWidget::onColorChanged);
    }
    if (ui->color_max_spin) {
        connect(ui->color_max_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EventPlotPropertiesWidget::onColorChanged);
    }
}

void EventPlotPropertiesWidget::setupYAxisFeatureTable() {
    if (ui->y_axis_feature_table) {
        // Set up columns for the Y-axis feature table
        ui->y_axis_feature_table->setColumns({"Feature", "Type", "Enabled"});

        // Filter to only show DigitalEventSeries and DigitalIntervalSeries
        ui->y_axis_feature_table->setTypeFilter({DM_DataType::DigitalEvent, DM_DataType::DigitalInterval});

        // Connect signals from the feature table
        connect(ui->y_axis_feature_table, &Feature_Table_Widget::featureSelected,
                this, &EventPlotPropertiesWidget::onYAxisFeatureSelected);
        connect(ui->y_axis_feature_table, &Feature_Table_Widget::addFeature,
                this, &EventPlotPropertiesWidget::onYAxisFeatureAdded);
        connect(ui->y_axis_feature_table, &Feature_Table_Widget::removeFeature,
                this, &EventPlotPropertiesWidget::onYAxisFeatureRemoved);
    }
}

QString EventPlotPropertiesWidget::getSelectedXAxisDataSource() const {
    if (ui->x_axis_combo) {
        return ui->x_axis_combo->currentData().toString();
    }
    return QString();
}

void EventPlotPropertiesWidget::setSelectedXAxisDataSource(QString const & data_key) {
    if (ui->x_axis_combo) {
        int index = ui->x_axis_combo->findData(data_key);
        if (index >= 0) {
            ui->x_axis_combo->setCurrentIndex(index);
        }
    }
}

bool EventPlotPropertiesWidget::isIntervalBeginningSelected() const {
    if (ui->interval_beginning_radio) {
        return ui->interval_beginning_radio->isChecked();
    }
    return true;// Default to beginning
}

QStringList EventPlotPropertiesWidget::getSelectedYAxisFeatures() const {
    return QStringList(_selected_y_axis_features.begin(), _selected_y_axis_features.end());
}

void EventPlotPropertiesWidget::updateIntervalSettingsVisibility() {
    if (!ui->interval_settings_group) {
        return;
    }

    QString selected_key = getSelectedXAxisDataSource();
    if (selected_key.isEmpty()) {
        ui->interval_settings_group->setVisible(false);
        return;
    }

    if (!_data_manager) {
        ui->interval_settings_group->setVisible(false);
        return;
    }

    DM_DataType data_type = _data_manager->getType(selected_key.toStdString());
    bool is_interval = (data_type == DM_DataType::DigitalInterval);
    ui->interval_settings_group->setVisible(is_interval);
}

void EventPlotPropertiesWidget::updateXAxisInfoLabel() {
    if (!ui->x_axis_info_label) {
        return;
    }

    QString selected_key = getSelectedXAxisDataSource();
    if (selected_key.isEmpty()) {
        ui->x_axis_info_label->setText("Select a data source for the X-axis");
        return;
    }

    if (!_data_manager) {
        ui->x_axis_info_label->setText("Data manager not available");
        return;
    }

    DM_DataType data_type = _data_manager->getType(selected_key.toStdString());
    QString info_text;

    if (data_type == DM_DataType::DigitalEvent) {
        info_text = QString("X-axis: %1 (Digital Event Series)\n"
                            "Events will be plotted at their exact time points.")
                            .arg(selected_key);
    } else if (data_type == DM_DataType::DigitalInterval) {
        QString interval_type = isIntervalBeginningSelected() ? "beginning" : "end";
        info_text = QString("X-axis: %1 (Digital Interval Series)\n"
                            "Intervals will be plotted at their %2 points.")
                            .arg(selected_key, interval_type);
    } else {
        info_text = QString("X-axis: %1\n"
                            "Data type not supported for event plotting.")
                            .arg(selected_key);
    }

    ui->x_axis_info_label->setText(info_text);
}

void EventPlotPropertiesWidget::updatePlotWidget() {
    qDebug() << "EventPlotPropertiesWidget::updatePlotWidget called";
    
    if (!_event_plot_widget) {
        qDebug() << "EventPlotPropertiesWidget::updatePlotWidget - no event plot widget";
        return;
    }

    // Clear legacy data keys - we're now using table data
    _event_plot_widget->setEventDataKeys(QStringList());
    _event_plot_widget->setYAxisDataKeys(QStringList());

    // Update X-axis range using capture range (±N samples)
    int capture_range = getCaptureRange();
    _event_plot_widget->setXAxisRange(capture_range, capture_range);

    // Update view bounds labels after setting the range
    updateViewBoundsLabels();

    // Load data from selected table instead of using legacy approach
    QString table_id = getSelectedTableId();
    QString column_name = getSelectedColumnName();
    
    qDebug() << "EventPlotPropertiesWidget::updatePlotWidget - table_id:" << table_id << "column_name:" << column_name;
    
    if (!table_id.isEmpty() && !column_name.isEmpty()) {
        loadTableData(table_id, column_name);
    } else {
        qDebug() << "EventPlotPropertiesWidget::updatePlotWidget - table_id or column_name is empty, skipping data load";
    }

    // Only emit properties changed signal when not applying properties
    // (to prevent infinite loop when applyToPlot() calls updatePlotWidget())
    if (!_applying_properties) {
        emit propertiesChanged();
    }
}

void EventPlotPropertiesWidget::onCaptureRangeChanged(int value) {
    Q_UNUSED(value)
    updatePlotWidget();
}

void EventPlotPropertiesWidget::onViewBoundsChanged() {
    updateViewBoundsLabels();
}

int EventPlotPropertiesWidget::getCaptureRange() const {
    if (ui->capture_range_spinbox) {
        return ui->capture_range_spinbox->value();
    }
    return 30000;// Default value
}

void EventPlotPropertiesWidget::setCaptureRange(int value) {
    if (ui->capture_range_spinbox) {
        ui->capture_range_spinbox->blockSignals(true);
        ui->capture_range_spinbox->setValue(value);
        ui->capture_range_spinbox->blockSignals(false);
    }
}

void EventPlotPropertiesWidget::updateViewBoundsLabels() {
    if (!_event_plot_widget || !_event_plot_widget->getOpenGLWidget()) {
        return;
    }

    // Get current visible bounds from the OpenGL widget (includes pan offset)
    float left_bound, right_bound;
    _event_plot_widget->getOpenGLWidget()->getVisibleBounds(left_bound, right_bound);

    // Update the labels with current visible bounds
    if (ui->left_bound_label) {
        ui->left_bound_label->setText(QString::number(static_cast<int>(left_bound)));
    }
    if (ui->right_bound_label) {
        ui->right_bound_label->setText(QString::number(static_cast<int>(right_bound)));
    }
}

void EventPlotPropertiesWidget::onTableSelectionChanged() {
    updateAvailableColumns();
    updateAvailableSortColumns();
    // Populate filter/color combos with same column list
    if (ui->filter_column_combo) {
        ui->filter_column_combo->clear();
        ui->filter_column_combo->addItem("Select column...", "");
        if (ui->sort_primary_combo) {
            for (int i = 1; i < ui->sort_primary_combo->count(); ++i) {
                ui->filter_column_combo->addItem(ui->sort_primary_combo->itemText(i), ui->sort_primary_combo->itemData(i));
            }
        }
    }
    if (ui->color_column_combo) {
        ui->color_column_combo->clear();
        ui->color_column_combo->addItem("Select column...", "");
        if (ui->sort_primary_combo) {
            for (int i = 1; i < ui->sort_primary_combo->count(); ++i) {
                ui->color_column_combo->addItem(ui->sort_primary_combo->itemText(i), ui->sort_primary_combo->itemData(i));
            }
        }
    }
    updatePlotWidget();
}

void EventPlotPropertiesWidget::onColumnSelectionChanged() {
    updatePlotWidget();
}

QString EventPlotPropertiesWidget::getSelectedTableId() const {
    if (ui->table_combo) {
        return ui->table_combo->currentData().toString();
    }
    return QString();
}

void EventPlotPropertiesWidget::setSelectedTableId(const QString& table_id) {
    if (ui->table_combo) {
        int index = ui->table_combo->findData(table_id);
        if (index >= 0) {
            ui->table_combo->setCurrentIndex(index);
        }
    }
}

QString EventPlotPropertiesWidget::getSelectedColumnName() const {
    if (ui->column_combo) {
        return ui->column_combo->currentData().toString();
    }
    return QString();
}

void EventPlotPropertiesWidget::setSelectedColumnName(const QString& column_name) {
    if (ui->column_combo) {
        int index = ui->column_combo->findData(column_name);
        if (index >= 0) {
            ui->column_combo->setCurrentIndex(index);
        }
    }
}

void EventPlotPropertiesWidget::updateAvailableTables() {
    if (!ui->table_combo || !_data_manager) {
        return;
    }

    ui->table_combo->clear();
    ui->table_combo->addItem("Select a table...", "");

    auto * regTables = _data_manager->getTableRegistry();
    auto table_ids = regTables ? regTables->getTableIds() : std::vector<QString>{};

    if (table_ids.empty()) {
        ui->table_combo->addItem("No tables available", "");
        ui->table_info_label->setText("Create tables using the Table Designer widget.");
    } else {
        for (const auto& table_id : table_ids) {
            ui->table_combo->addItem(table_id, table_id);
        }
        ui->table_info_label->setText(QString("Found %1 tables with built data.").arg(table_ids.size()));
    }

    // Update columns for the first valid table if any
    updateAvailableColumns();
    updateAvailableSortColumns();
}

void EventPlotPropertiesWidget::updateAvailableColumns() {
    if (!ui->column_combo || !_data_manager) {
        return;
    }

    ui->column_combo->clear();
    ui->column_combo->addItem("Select a column...", "");

    QString selected_table_id = getSelectedTableId();
    if (selected_table_id.isEmpty()) {
        return;
    }

    auto * regA = _data_manager->getTableRegistry();
    auto table_view = regA->getBuiltTable(selected_table_id);
    if (!table_view) {
        ui->column_combo->addItem("Table not built", "");
        return;
    }

    auto column_names = table_view->getColumnNames();
    for (const auto& column_name : column_names) {
        QString qname = QString::fromStdString(column_name);
        ui->column_combo->addItem(qname, qname);
    }
}

void EventPlotPropertiesWidget::updateAvailableSortColumns() {
    if (!_data_manager) return;
    if (!ui->sort_primary_combo || !ui->sort_secondary_combo) return;

    ui->sort_primary_combo->clear();
    ui->sort_secondary_combo->clear();

    ui->sort_primary_combo->addItem("Select column...", "");
    ui->sort_secondary_combo->addItem("None", "");

    auto * regB = _data_manager->getTableRegistry();
    if (!regB) return;

    // Row count should match the selected table
    QString base_table_id = getSelectedTableId();
    if (base_table_id.isEmpty()) return;
    auto base_view = regB->getBuiltTable(base_table_id);
    if (!base_view) return;
    size_t row_count = base_view->getRowCount();

    auto table_ids = regB->getTableIds();
    for (auto const & table_id : table_ids) {
        auto view = regB->getBuiltTable(table_id);
        if (!view) continue;
        if (view->getRowCount() != row_count) continue; // must match rows

        auto column_names = view->getColumnNames();
        for (auto const & column_name : column_names) {
            // Only scalar numeric elemental types: float, double, int, bool
            try {
                auto idx = view->getColumnTypeIndex(column_name);
                bool is_scalar_numeric = (idx == typeid(float) || idx == typeid(double) || idx == typeid(int) || idx == typeid(bool));
                if (!is_scalar_numeric) continue;
            } catch (...) {
                continue;
            }
            

            QString label = QString("%1.%2").arg(table_id).arg(QString::fromStdString(column_name));
            QString key = QString("table:%1:%2").arg(table_id).arg(QString::fromStdString(column_name));
            ui->sort_primary_combo->addItem(label, key);
            ui->sort_secondary_combo->addItem(label, key);
        }
    }
}

std::pair<QString, QString> EventPlotPropertiesWidget::parseSortKey(QString const & key) {
    if (!key.startsWith("table:")) return {QString(), QString()};
    auto parts = key.split(":");
    if (parts.size() < 3) return {QString(), QString()};
    QString table_id = parts.value(1);
    // column name may include ':' in theory; join remainder
    QString column_name = parts.mid(2).join(":");
    return {table_id, column_name};
}

// removed local helpers in favor of shared pipeline

void EventPlotPropertiesWidget::onSortingToggled(bool enabled) {
    _sorting_enabled = enabled;
    rebuildPipeline();
    onSortingChanged();
}

void EventPlotPropertiesWidget::onSortingChanged() {
    if (ui->sort_primary_combo) _sort_primary_key = ui->sort_primary_combo->currentData().toString();
    if (ui->sort_secondary_combo) _sort_secondary_key = ui->sort_secondary_combo->currentData().toString();
    if (ui->sort_order_combo) _sort_order_index = ui->sort_order_combo->currentIndex();
    rebuildPipeline();
    if (!_applying_properties) updatePlotWidget();
}

void EventPlotPropertiesWidget::onFilterToggled(bool enabled) {
    Q_UNUSED(enabled)
    rebuildPipeline();
    if (!_applying_properties) updatePlotWidget();
}

void EventPlotPropertiesWidget::onFilterChanged() {
    rebuildPipeline();
    if (!_applying_properties) updatePlotWidget();
}

void EventPlotPropertiesWidget::onColorToggled(bool enabled) {
    Q_UNUSED(enabled)
    rebuildPipeline();
    if (!_applying_properties) updatePlotWidget();
}

void EventPlotPropertiesWidget::onColorChanged() {
    rebuildPipeline();
    if (!_applying_properties) updatePlotWidget();
}

void EventPlotPropertiesWidget::rebuildPipeline() {
    _pipeline.clear();
    // Filter first
    if (ui->filter_enabled_checkbox && ui->filter_enabled_checkbox->isChecked()) {
        if (ui->filter_column_combo && !ui->filter_column_combo->currentData().toString().isEmpty()) {
            auto filter = std::make_unique<FilterByRangeTransform>();
            auto [ftid, fcol] = parseSortKey(ui->filter_column_combo->currentData().toString());
            filter->tableId = ftid;
            filter->columnName = fcol;
            int cmpIdx = ui->filter_comparator_combo ? ui->filter_comparator_combo->currentIndex() : 2; // default '>'
            switch (cmpIdx) {
                case 0: filter->comparator = FilterByRangeTransform::Comparator::LT; break;
                case 1: filter->comparator = FilterByRangeTransform::Comparator::LE; break;
                case 2: filter->comparator = FilterByRangeTransform::Comparator::GT; break;
                case 3: filter->comparator = FilterByRangeTransform::Comparator::GE; break;
                case 4: filter->comparator = FilterByRangeTransform::Comparator::EQ; break;
                case 5: filter->comparator = FilterByRangeTransform::Comparator::NE; break;
                default: filter->comparator = FilterByRangeTransform::Comparator::GT; break;
            }
            filter->value = ui->filter_value_spin ? ui->filter_value_spin->value() : 0.0;
            _pipeline.addTransform(std::move(filter));
        }
    }
    if (_sorting_enabled) {
        auto sort = std::make_unique<SortByColumnTransform>();
        // Parse primary
        if (!ui->sort_primary_combo || ui->sort_primary_combo->currentData().toString().isEmpty()) {
            // no-op
        } else {
            auto [ptid, pcol] = parseSortKey(ui->sort_primary_combo->currentData().toString());
            sort->tableIdPrimary = ptid;
            sort->columnPrimary = pcol;
        }
        // Parse secondary
        if (ui->sort_secondary_combo && !ui->sort_secondary_combo->currentData().toString().isEmpty()) {
            auto [stid, scol] = parseSortKey(ui->sort_secondary_combo->currentData().toString());
            sort->tableIdSecondary = stid;
            sort->columnSecondary = scol;
        }
        sort->order = (_sort_order_index == 0) ? SortByColumnTransform::Order::Asc : SortByColumnTransform::Order::Desc;
        _pipeline.addTransform(std::move(sort));
    }
    // Color last
    if (ui->color_enabled_checkbox && ui->color_enabled_checkbox->isChecked()) {
        if (ui->color_column_combo && !ui->color_column_combo->currentData().toString().isEmpty()) {
            auto color = std::make_unique<ColorByFeatureTransform>();
            auto [ctid, ccol] = parseSortKey(ui->color_column_combo->currentData().toString());
            color->tableId = ctid;
            color->columnName = ccol;
            int modeIdx = ui->color_mode_combo ? ui->color_mode_combo->currentIndex() : 0;
            color->mode = (modeIdx == 0) ? ColorByFeatureTransform::Mode::DiscreteBool : (modeIdx == 1 ? ColorByFeatureTransform::Mode::DiscreteInt : ColorByFeatureTransform::Mode::ContinuousFloat);
            // discrete defaults
            if (ui->color_false_spin && ui->color_true_spin) {
                color->discreteMap.clear();
                color->discreteMap[0] = static_cast<uint32_t>(ui->color_false_spin->value());
                color->discreteMap[1] = static_cast<uint32_t>(ui->color_true_spin->value());
            }
            // continuous defaults
            if (ui->color_min_spin && ui->color_max_spin) {
                color->minValue = ui->color_min_spin->value();
                color->maxValue = ui->color_max_spin->value();
            }
            _pipeline.addTransform(std::move(color));
        }
    }
}

void EventPlotPropertiesWidget::loadTableData(const QString& table_id, const QString& column_name) {
    qDebug() << "EventPlotPropertiesWidget::loadTableData called with table_id:" << table_id << "column_name:" << column_name;
    
    if (!_data_manager || !_event_plot_widget) {
        qWarning() << "EventPlotPropertiesWidget: Missing data_source_registry or event_plot_widget";
        return;
    }

    // Get the typed data - try std::vector<float> first (common for event data)
    try {
        qDebug() << "EventPlotPropertiesWidget: Attempting to load table data...";
        auto * reg0 = _data_manager->getTableRegistry();
        auto view0 = reg0 ? reg0->getBuiltTable(table_id) : nullptr;
        std::vector<std::vector<float>> event_data;
        if (view0) {
            auto variant = view0->getColumnDataVariant(column_name.toStdString());
            // Try to extract vector<vector<float>> or compatible
            if (auto p = std::get_if<std::vector<std::vector<float>>>(&variant)) {
                event_data = *p;
            }
        }
        
        qDebug() << "EventPlotPropertiesWidget: Retrieved" << event_data.size() << "event vectors";
        
        if (!event_data.empty()) {
            // Evaluate pipeline to get row order/mask, then apply to event_data
            if (_data_manager) {
                auto * reg2 = _data_manager->getTableRegistry();
                auto view2 = reg2 ? reg2->getBuiltTable(table_id) : nullptr;
                if (view2) {
                    DataViewContext ctx;
                    ctx.tableId = table_id;
                    ctx.tableView = view2;
                    ctx.tableRegistry = reg2;
                    ctx.rowCount = event_data.size();

                    auto state = _pipeline.evaluate(ctx);

                    // Apply mask and order; track row color indices if present
                    std::vector<std::vector<float>> filtered;
                    filtered.reserve(event_data.size());
                    std::vector<uint32_t> row_colors_reordered;
                    if (state.rowColorIndices && !state.rowColorIndices->empty()) {
                        row_colors_reordered.reserve(event_data.size());
                    }
                    for (size_t idx : state.rowOrder) {
                        if (idx < event_data.size() && (idx < state.rowMask.size() ? state.rowMask[idx] != 0 : true)) {
                            filtered.push_back(std::move(event_data[idx]));
                            if (state.rowColorIndices && idx < state.rowColorIndices->size()) {
                                row_colors_reordered.push_back((*state.rowColorIndices)[idx]);
                            }
                        }
                    }
                    event_data = std::move(filtered);
                    // Propagate row colors to OpenGL widget after setEventData
                    if (!row_colors_reordered.empty()) {
                        if (_event_plot_widget && _event_plot_widget->getOpenGLWidget()) {
                            _event_plot_widget->getOpenGLWidget()->setRowColorIndices(row_colors_reordered);
                        }
                    }
                }
            }
            // Pass the event data directly to the OpenGL widget
            if (_event_plot_widget->getOpenGLWidget()) {
                _event_plot_widget->getOpenGLWidget()->setEventData(event_data);
                qDebug() << "EventPlotPropertiesWidget: Successfully loaded" << event_data.size() << "event vectors from table" << table_id << "column" << column_name;
            } else {
                qWarning() << "EventPlotPropertiesWidget: OpenGL widget is null";
            }
        } else {
            qWarning() << "EventPlotPropertiesWidget: No data found for table" << table_id << "column" << column_name;
        }
    } catch (const std::exception& e) {
        qWarning() << "EventPlotPropertiesWidget: Failed to load table data:" << e.what();
    }
}