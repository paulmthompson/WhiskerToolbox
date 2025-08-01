
#include "ScatterPlotPropertiesWidget.hpp"
#include "ScatterPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "ui_ScatterPlotPropertiesWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>


ScatterPlotPropertiesWidget::ScatterPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::ScatterPlotPropertiesWidget),
      _scatter_plot_widget(nullptr) {
    ui->setupUi(this);
    setupConnections();
}

ScatterPlotPropertiesWidget::~ScatterPlotPropertiesWidget() {
    delete ui;
}

void ScatterPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = data_manager;
    updateAvailableDataSources();
}

void ScatterPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    _plot_widget = plot_widget;
    _scatter_plot_widget = qobject_cast<ScatterPlotWidget *>(plot_widget);

    if (_scatter_plot_widget) {
        updateFromPlot();
    }
}

void ScatterPlotPropertiesWidget::updateFromPlot() {
    if (!_scatter_plot_widget) {
        return;
    }

    // Update UI with current plot settings
    // For now, just set default values since ScatterPlotWidget doesn't have specific properties yet

    // Set default point size
    ui->point_size_spinbox->setValue(3.0);

    // Set default color
    ui->point_color_button->setStyleSheet("background-color: #3268a8; border: 1px solid #ccc;");

    // Set default grid and legend visibility
    ui->show_grid_checkbox->setChecked(true);
    ui->show_legend_checkbox->setChecked(true);
}

void ScatterPlotPropertiesWidget::applyToPlot() {
    if (!_scatter_plot_widget) {
        return;
    }

    // Apply current settings to the plot widget
    updatePlotWidget();
}

void ScatterPlotPropertiesWidget::updateAvailableDataSources() {
    if (!_data_manager || !ui->x_axis_combo || !ui->y_axis_combo) {
        return;
    }

    // Clear existing items
    ui->x_axis_combo->clear();
    ui->y_axis_combo->clear();

    ui->x_axis_combo->addItem("Select a data source...", "");
    ui->y_axis_combo->addItem("Select a data source...", "");

    // Get all available keys from DataManager
    std::vector<std::string> all_keys = _data_manager->getAllKeys();

    for (std::string const & key: all_keys) {
        DM_DataType data_type = _data_manager->getType(key);
        QString display_text = QString::fromStdString(key);

        // Add to both X and Y axis combos for now
        // In a real implementation, you might filter based on data type
        ui->x_axis_combo->addItem(display_text, QString::fromStdString(key));
        ui->y_axis_combo->addItem(display_text, QString::fromStdString(key));
    }

    // Update info labels
    updateXAxisInfoLabel();
    updateYAxisInfoLabel();
}

void ScatterPlotPropertiesWidget::onXAxisDataSourceChanged() {
    updateXAxisInfoLabel();
    updatePlotWidget();
}

void ScatterPlotPropertiesWidget::onYAxisDataSourceChanged() {
    updateYAxisInfoLabel();
    updatePlotWidget();
}

void ScatterPlotPropertiesWidget::onPointSizeChanged(double value) {
    Q_UNUSED(value)
    updatePlotWidget();
}

void ScatterPlotPropertiesWidget::onPointColorChanged() {
    QColor current_color = QColor(50, 104, 168);// Default blue color

    QColorDialog color_dialog(this);
    color_dialog.setCurrentColor(current_color);

    if (color_dialog.exec() == QDialog::Accepted) {
        QColor selected_color = color_dialog.selectedColor();
        ui->point_color_button->setStyleSheet(
                QString("background-color: %1; border: 1px solid #ccc;")
                        .arg(selected_color.name()));
        updatePlotWidget();
    }
}

void ScatterPlotPropertiesWidget::onShowGridToggled(bool enabled) {
    Q_UNUSED(enabled)
    updatePlotWidget();
}

void ScatterPlotPropertiesWidget::onShowLegendToggled(bool enabled) {
    Q_UNUSED(enabled)
    updatePlotWidget();
}

void ScatterPlotPropertiesWidget::setupConnections() {
    // Connect UI signals to slots
    if (ui->x_axis_combo) {
        connect(ui->x_axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ScatterPlotPropertiesWidget::onXAxisDataSourceChanged);
    }

    if (ui->y_axis_combo) {
        connect(ui->y_axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ScatterPlotPropertiesWidget::onYAxisDataSourceChanged);
    }

    if (ui->point_size_spinbox) {
        connect(ui->point_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &ScatterPlotPropertiesWidget::onPointSizeChanged);
    }

    if (ui->point_color_button) {
        connect(ui->point_color_button, &QPushButton::clicked,
                this, &ScatterPlotPropertiesWidget::onPointColorChanged);
    }

    if (ui->show_grid_checkbox) {
        connect(ui->show_grid_checkbox, &QCheckBox::toggled,
                this, &ScatterPlotPropertiesWidget::onShowGridToggled);
    }

    if (ui->show_legend_checkbox) {
        connect(ui->show_legend_checkbox, &QCheckBox::toggled,
                this, &ScatterPlotPropertiesWidget::onShowLegendToggled);
    }
}

QString ScatterPlotPropertiesWidget::getSelectedXAxisDataSource() const {
    if (ui->x_axis_combo) {
        return ui->x_axis_combo->currentData().toString();
    }
    return QString();
}

void ScatterPlotPropertiesWidget::setSelectedXAxisDataSource(QString const & data_key) {
    if (ui->x_axis_combo) {
        int index = ui->x_axis_combo->findData(data_key);
        if (index >= 0) {
            ui->x_axis_combo->setCurrentIndex(index);
        }
    }
}

QString ScatterPlotPropertiesWidget::getSelectedYAxisDataSource() const {
    if (ui->y_axis_combo) {
        return ui->y_axis_combo->currentData().toString();
    }
    return QString();
}

void ScatterPlotPropertiesWidget::setSelectedYAxisDataSource(QString const & data_key) {
    if (ui->y_axis_combo) {
        int index = ui->y_axis_combo->findData(data_key);
        if (index >= 0) {
            ui->y_axis_combo->setCurrentIndex(index);
        }
    }
}

void ScatterPlotPropertiesWidget::updatePlotWidget() {
    if (!_scatter_plot_widget) {
        return;
    }

    // For now, just emit properties changed signal
    // In a real implementation, you would update the plot widget with the current settings
    emit propertiesChanged();
}

void ScatterPlotPropertiesWidget::updateXAxisInfoLabel() {
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
    QString info_text = QString("X-axis: %1\n"
                                "Data type: %2")
                                .arg(selected_key, QString::fromStdString(convert_data_type_to_string(data_type)));

    ui->x_axis_info_label->setText(info_text);
}

void ScatterPlotPropertiesWidget::updateYAxisInfoLabel() {
    if (!ui->y_axis_info_label) {
        return;
    }

    QString selected_key = getSelectedYAxisDataSource();
    if (selected_key.isEmpty()) {
        ui->y_axis_info_label->setText("Select a data source for the Y-axis");
        return;
    }

    if (!_data_manager) {
        ui->y_axis_info_label->setText("Data manager not available");
        return;
    }

    DM_DataType data_type = _data_manager->getType(selected_key.toStdString());
    QString info_text = QString("Y-axis: %1\n"
                                "Data type: %2")
                                .arg(selected_key, QString::fromStdString(convert_data_type_to_string(data_type)));

    ui->y_axis_info_label->setText(info_text);
}