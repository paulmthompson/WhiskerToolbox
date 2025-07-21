#include "EventPlotPropertiesWidget.hpp"
#include "EventPlotOpenGLWidget.hpp"
#include "EventPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "ui_EventPlotPropertiesWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>


EventPlotPropertiesWidget::EventPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _event_plot_widget(nullptr) {
    ui->setupUi(this);
    setupConnections();
    setupYAxisFeatureTable();
}

EventPlotPropertiesWidget::~EventPlotPropertiesWidget() {
    delete ui;
}

void EventPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = data_manager;
    updateAvailableDataSources();
    updateIntervalSettingsVisibility();

    // Set up Y-axis feature table with data manager
    if (ui->y_axis_feature_table) {
        ui->y_axis_feature_table->setDataManager(_data_manager);
        ui->y_axis_feature_table->populateTable();
    }
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

    // Apply current settings to the plot widget
    updatePlotWidget();
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
    if (!_event_plot_widget) {
        return;
    }

    // Update event data keys with selected X-axis source
    QString selected_source = getSelectedXAxisDataSource();
    QStringList selected_sources;
    if (!selected_source.isEmpty()) {
        selected_sources.append(selected_source);
    }

    _event_plot_widget->setEventDataKeys(selected_sources);

    // Update X-axis range using capture range (±N samples)
    int capture_range = getCaptureRange();
    _event_plot_widget->setXAxisRange(capture_range, capture_range);

    // Update view bounds labels after setting the range
    updateViewBoundsLabels();

    QStringList y_axis_features = getSelectedYAxisFeatures();
    _event_plot_widget->setYAxisDataKeys(y_axis_features);

    // Emit properties changed signal
    emit propertiesChanged();
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