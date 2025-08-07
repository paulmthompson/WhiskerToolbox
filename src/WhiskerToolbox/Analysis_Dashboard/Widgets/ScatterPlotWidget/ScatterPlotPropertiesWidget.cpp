
#include "ScatterPlotPropertiesWidget.hpp"
#include "ScatterPlotWidget.hpp"

#include "DataManager/utils/TableView/columns/ColumnTypeInfo.hpp"
#include "DataSourceRegistry/DataSourceRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "DataManager/utils/TableView/core/TableView.h"
#include "Analysis_Dashboard/Tables/TableManager.hpp"
#include "ScatterPlotOpenGLWidget.hpp"
#include "ui_ScatterPlotPropertiesWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <numeric>

ScatterPlotPropertiesWidget::ScatterPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::ScatterPlotPropertiesWidget),
      _scatter_plot_widget(nullptr),
      _data_source_registry(nullptr),
      _applying_properties(false) {
    ui->setupUi(this);
    setupConnections();
}

ScatterPlotPropertiesWidget::~ScatterPlotPropertiesWidget() {
    delete ui;
}

void ScatterPlotPropertiesWidget::setDataSourceRegistry(DataSourceRegistry * data_source_registry) {
    _data_source_registry = data_source_registry;
    
    if (_data_source_registry) {
        // Connect to data source registry signals for dynamic updates
        connect(_data_source_registry, &DataSourceRegistry::dataSourceRegistered,
                this, &ScatterPlotPropertiesWidget::updateAvailableDataSources);
        connect(_data_source_registry, &DataSourceRegistry::dataSourceUnregistered,
                this, &ScatterPlotPropertiesWidget::updateAvailableDataSources);
    }
    
    updateAvailableDataSources();
}

void ScatterPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    _plot_widget = plot_widget;
    _scatter_plot_widget = qobject_cast<ScatterPlotWidget *>(plot_widget);

    if (_scatter_plot_widget) {
        updateFromPlot();

        // Connect to property changes to update the UI
        if (_scatter_plot_widget->getOpenGLWidget()) {
            connect(_scatter_plot_widget->getOpenGLWidget(), &ScatterPlotOpenGLWidget::zoomLevelChanged,
                    this, &ScatterPlotPropertiesWidget::updateFromPlot);
            connect(_scatter_plot_widget->getOpenGLWidget(), &ScatterPlotOpenGLWidget::panOffsetChanged,
                    this, &ScatterPlotPropertiesWidget::updateFromPlot);
        }
    }
}

void ScatterPlotPropertiesWidget::setScatterPlotWidget(ScatterPlotWidget * scatter_widget) {
    _scatter_plot_widget = scatter_widget;
    updateFromPlot();
}

void ScatterPlotPropertiesWidget::updateFromPlot() {
    if (!_scatter_plot_widget) {
        return;
    }

    // Update UI with current plot settings
    ui->point_size_spinbox->setValue(_scatter_plot_widget->getPointSize());
    ui->point_color_button->setStyleSheet("background-color: #3268a8; border: 1px solid #ccc;");
    ui->show_grid_checkbox->setChecked(true);
    ui->show_legend_checkbox->setChecked(true);
}

void ScatterPlotPropertiesWidget::applyToPlot() {
    if (!_scatter_plot_widget) {
        return;
    }

    _applying_properties = true;
    updatePlotWidget();
    _applying_properties = false;
}

void ScatterPlotPropertiesWidget::updateAvailableDataSources() {
    if (!ui->x_axis_combo || !ui->y_axis_combo) {
        return;
    }

    if (!_data_source_registry) {
        std::cout << "ScatterPlotPropertiesWidget::updateAvailableDataSources - Data source registry is null" << std::endl;
        return;
    }

    qDebug() << "ScatterPlotPropertiesWidget::updateAvailableDataSources: Starting update";

    // Clear existing items
    ui->x_axis_combo->clear();
    ui->y_axis_combo->clear();

    ui->x_axis_combo->addItem("Select a data source...", "");
    ui->y_axis_combo->addItem("Select a data source...", "");

    auto data_manager_source = _data_source_registry->getDataSource("primary_data_manager");
    auto data_manager = data_manager_source ? dynamic_cast<DataManagerSource*>(data_manager_source)->getDataManager() : nullptr;

    // Add items from DataManager (analog time series)
    if (!data_manager) {
        std::cout << "ScatterPlotPropertiesWidget::updateAvailableDataSources - Data manager is null" << std::endl;
        return;
    }

    std::vector<std::string> all_keys = data_manager->getAllKeys();
        
    for (std::string const & key: all_keys) {
        DM_DataType data_type = data_manager->getType(key);
            
        // Only add analog time series for scatter plots
        if (data_type == DM_DataType::Analog) {
            QString display_text = QString("Analog: %1").arg(QString::fromStdString(key));
            QString data_key = QString("analog:%1").arg(QString::fromStdString(key));
                
            ui->x_axis_combo->addItem(display_text, data_key);
            ui->y_axis_combo->addItem(display_text, data_key);
                
            qDebug() << "Added analog time series:" << display_text;
        }
    }

    // Add items from TableManager (table columns)
    // Find TableManagerSource
    AbstractDataSource* table_manager_source = nullptr;
    auto source_ids = _data_source_registry->getAvailableSourceIds();

    for (auto const & source_id : source_ids) {
        auto* source = _data_source_registry->getDataSource(source_id);
        if (source && source->getType() == "TableManager") {
            table_manager_source = source;
            break;
        }
    }

    
        if (table_manager_source) {
            TableManagerSource* tm_source = static_cast<TableManagerSource*>(table_manager_source);
            auto table_manager = tm_source->getTableManager();
            
            if (table_manager) {
                auto table_ids = table_manager->getTableIds();
                qDebug() << "Found" << table_ids.size() << "tables in table manager";
                
                for (auto const & table_id : table_ids) {
                    qDebug() << "Processing table:" << table_id;
                    
                    auto table_view = table_manager->getBuiltTable(table_id);
                    if (table_view) {
                        qDebug() << "Table" << table_id << "is built and available";
                        auto column_names = table_view->getColumnNames();
                        qDebug() << "Table" << table_id << "has" << column_names.size() << "columns:" << QString::fromStdString(std::accumulate(column_names.begin(), column_names.end(), std::string{}, [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; }));
                        
                        for (auto const & column_name : column_names) {
                            // Use our new type-safe interface to filter by numeric types
                            ColumnTypeInfo type_info = tm_source->getColumnTypeInfo(table_id, QString::fromStdString(column_name));
                            
                            qDebug() << "Column:" << QString::fromStdString(column_name)
                                     << "Type:" << QString::fromStdString(type_info.typeName)
                                     << "ElementType:" << QString::fromStdString(type_info.elementTypeName)
                                     << "isVectorType:" << type_info.isVectorType
                                     << "hasElementType<double>():" << type_info.hasElementType<double>()
                                     << "hasElementType<float>():" << type_info.hasElementType<float>()
                                     << "hasElementType<int>():" << type_info.hasElementType<int>();
                            
                            // Only add columns that contain numeric data suitable for scatter plots
                            // We want std::vector<double> etc., but NOT std::vector<std::vector<double>>
                            bool is_plottable_numeric = false;
                            if (!type_info.isVectorType) {
                                // Check if it's a vector of numeric types (single values per row)
                                is_plottable_numeric = type_info.hasElementType<float>() || 
                                                     type_info.hasElementType<double>() || 
                                                     type_info.hasElementType<int>();
                            }
                            
                            // Special handling for void types - they might not be built yet
                            if (type_info.typeName == "void") {
                                qDebug() << "Column" << QString::fromStdString(column_name) 
                                        << "has void type - table may not be built yet";
                            }
                            
                            if (is_plottable_numeric) {
                                QString type_display = QString::fromStdString(type_info.elementTypeName);
                                QString display_text = QString("Table: %1.%2 (%3)")
                                                       .arg(table_id)
                                                       .arg(QString::fromStdString(column_name))
                                                       .arg(type_display);
                                QString data_key = QString("table:%1:%2")
                                                   .arg(table_id)
                                                   .arg(QString::fromStdString(column_name));
                                
                                ui->x_axis_combo->addItem(display_text, data_key);
                                ui->y_axis_combo->addItem(display_text, data_key);
                                
                                qDebug() << "Added numeric table column:" << display_text 
                                        << "Type:" << QString::fromStdString(type_info.typeName);
                            } else {
                                qDebug() << "Skipped non-numeric column:" << QString::fromStdString(column_name)
                                        << "Type:" << QString::fromStdString(type_info.typeName)
                                        << "Reason: isVectorType=" << type_info.isVectorType
                                        << "isNumeric=" << (type_info.hasElementType<float>() || 
                                                           type_info.hasElementType<double>() || 
                                                           type_info.hasElementType<int>());
                            }
                        }
                    } else {
                        qDebug() << "Table" << table_id << "is not built yet";
                    }
                }
            } else {
                qDebug() << "TableManagerSource has no table manager";
            }
        } else {
            qDebug() << "No TableManagerSource found in data source registry";
        }

    qDebug() << "ScatterPlotPropertiesWidget::updateAvailableDataSources: Completed update";

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
    if (_scatter_plot_widget) {
        _scatter_plot_widget->setPointSize(static_cast<float>(value));
    }
    
    if (!_applying_properties) {
        updatePlotWidget();
    }
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
    qDebug() << "ScatterPlotPropertiesWidget::updatePlotWidget: Starting update";
    
    QString x_data_key = getSelectedXAxisDataSource();
    QString y_data_key = getSelectedYAxisDataSource();
    
    qDebug() << "Selected X data key:" << x_data_key;
    qDebug() << "Selected Y data key:" << y_data_key;
    
    if (x_data_key.isEmpty() || y_data_key.isEmpty()) {
        qDebug() << "One or both data keys are empty, skipping plot update";
        return;
    }

    // Load X and Y data
    std::vector<float> x_data = loadDataFromKey(x_data_key);
    std::vector<float> y_data = loadDataFromKey(y_data_key);
    
    qDebug() << "Loaded X data size:" << x_data.size();
    qDebug() << "Loaded Y data size:" << y_data.size();
    
    if (x_data.empty() || y_data.empty()) {
        qDebug() << "One or both data vectors are empty";
        return;
    }
    
    // Make sure both data vectors have the same size
    size_t min_size = std::min(x_data.size(), y_data.size());
    if (x_data.size() != y_data.size()) {
        qDebug() << "Data size mismatch, trimming to minimum size:" << min_size;
        x_data.resize(min_size);
        y_data.resize(min_size);
    }
    
    // Update the scatter plot widget if available
    if (_scatter_plot_widget) {
        _scatter_plot_widget->setScatterData(x_data, y_data);
        
        // Set axis labels based on data keys
        QString x_label = x_data_key.startsWith("analog:") ? 
                         x_data_key.mid(7) : 
                         (x_data_key.startsWith("table:") ? x_data_key.split(":").last() : x_data_key);
        QString y_label = y_data_key.startsWith("analog:") ? 
                         y_data_key.mid(7) : 
                         (y_data_key.startsWith("table:") ? y_data_key.split(":").last() : y_data_key);
                         
        _scatter_plot_widget->setAxisLabels(x_label, y_label);
        
        // Apply other settings
        _scatter_plot_widget->setPointSize(static_cast<float>(ui->point_size_spinbox->value()));
        
        qDebug() << "Updated scatter plot widget with" << min_size << "points";
    } else {
        qDebug() << "No scatter plot widget available, cannot update plot";
    }
    
    // Only emit properties changed signal when not applying properties
    if (!_applying_properties) {
        emit propertiesChanged();
    }
}

std::vector<float> ScatterPlotPropertiesWidget::loadDataFromKey(QString const & data_key) {
    std::vector<float> result;

    if (!_data_source_registry) {
        std::cout << "ScatterPlotPropertiesWidget::loadDataFromKey - Data source registry is null" << std::endl;
        return result;
    }
    
    if (data_key.startsWith("analog:")) {
        // Handle analog time series
        QString analog_key = data_key.mid(7); // Remove "analog:" prefix
        
        try {
             auto data_manager_source = _data_source_registry->getDataSource("primary_data_manager");
             auto data_manager = data_manager_source ? dynamic_cast<DataManagerSource*>(data_manager_source)->getDataManager() : nullptr;

            auto analog_data = data_manager ? data_manager->getData<AnalogTimeSeries>(analog_key.toStdString()) : nullptr;
            if (analog_data) {
                auto data_vector = analog_data->getAnalogTimeSeries();
                result.reserve(data_vector.size());
                
                for (auto value : data_vector) {
                    result.push_back(static_cast<float>(value));
                }
                
                qDebug() << "Loaded" << result.size() << "values from analog time series:" << analog_key;
            }
        } catch (std::exception const & e) {
            qDebug() << "Error loading analog data:" << e.what();
        }
        
    } else if (data_key.startsWith("table:")) {
        // Handle table column data
        QStringList parts = data_key.split(":");
        if (parts.size() >= 3) {
            QString table_id = parts[1];
            QString column_name = parts[2];
            
            if (!_data_source_registry) {
                qDebug() << "No data source registry available for table data";
                return result;
            }
            
            // Find TableManagerSource
            AbstractDataSource* table_manager_source = nullptr;
            auto source_ids = _data_source_registry->getAvailableSourceIds();

            for (auto const & source_id : source_ids) {
                auto* source = _data_source_registry->getDataSource(source_id);
                if (source && source->getType() == "TableManager") {
                    table_manager_source = source;
                    break;
                }
            }
            
            if (table_manager_source) {
                TableManagerSource* tm_source = static_cast<TableManagerSource*>(table_manager_source);
                
                try {
                    // Use the new type-safe interface
                    auto column_variant = tm_source->getTableColumnDataVariant(table_id, column_name);
                    
                    // Create a visitor to extract numeric data as float vector
                    auto float_vector_visitor = [&result](const auto& data) {
                        using DataType = std::decay_t<decltype(data)>;
                        
                        if constexpr (std::is_same_v<DataType, std::vector<float>>) {
                            result = data;
                        }
                        else if constexpr (std::is_same_v<DataType, std::vector<double>>) {
                            result.reserve(data.size());
                            for (const auto& value : data) {
                                result.push_back(static_cast<float>(value));
                            }
                        }
                        else if constexpr (std::is_same_v<DataType, std::vector<int>>) {
                            result.reserve(data.size());
                            for (const auto& value : data) {
                                result.push_back(static_cast<float>(value));
                            }
                        }
                        else if constexpr (std::is_same_v<DataType, float>) {
                            result.push_back(data);
                        }
                        else if constexpr (std::is_same_v<DataType, double>) {
                            result.push_back(static_cast<float>(data));
                        }
                        else if constexpr (std::is_same_v<DataType, int>) {
                            result.push_back(static_cast<float>(data));
                        }
                        else {
                            qWarning() << "Unsupported column data type for scatter plot";
                        }
                    };
                    
                    std::visit(float_vector_visitor, column_variant);
                    
                    qDebug() << "Loaded" << result.size() << "values from table column:" 
                             << table_id << "." << column_name;
                             
                } catch (std::exception const & e) {
                    qDebug() << "Error loading table column data:" << e.what();
                }
            }
        }
    }
    
    return result;
}

void ScatterPlotPropertiesWidget::updateXAxisInfoLabel() {
    if (!ui->x_axis_info_label) {
        return;
    }

    if (!_data_source_registry) {
        std::cout << "ScatterPlotPropertiesWidget::updateXAxisInfoLabel - Data source registry is null" << std::endl;
        return;
    }

    QString selected_key = getSelectedXAxisDataSource();
    if (selected_key.isEmpty()) {
        ui->x_axis_info_label->setText("Select a data source for the X-axis");
        return;
    }

    QString info_text;
    
    if (selected_key.startsWith("analog:")) {
        QString analog_key = selected_key.mid(7);
        info_text = QString("X-axis: Analog Time Series\nKey: %1").arg(analog_key);

        auto data_manager_source = _data_source_registry->getDataSource("primary_data_manager");
        auto data_manager = data_manager_source ? dynamic_cast<DataManagerSource*>(data_manager_source)->getDataManager() : nullptr;

        if (data_manager) {
            try {
                auto analog_data = data_manager->getData<AnalogTimeSeries>(analog_key.toStdString());
                if (analog_data) {
                    info_text += QString("\nSamples: %1").arg(analog_data->getNumSamples());
                }
            } catch (std::exception const & e) {
                info_text += QString("\nError: %1").arg(e.what());
            }
        }
    } else if (selected_key.startsWith("table:")) {
        QStringList parts = selected_key.split(":");
        if (parts.size() >= 3) {
            QString table_id = parts[1];
            QString column_name = parts[2];
            info_text = QString("X-axis: Table Column\nTable: %1\nColumn: %2").arg(table_id, column_name);
        }
    } else {
        info_text = QString("X-axis: Unknown data type\nKey: %1").arg(selected_key);
    }

    ui->x_axis_info_label->setText(info_text);
}

void ScatterPlotPropertiesWidget::updateYAxisInfoLabel() {
    if (!ui->y_axis_info_label) {
        return;
    }

    if (!_data_source_registry) {
        std::cout << "ScatterPlotPropertiesWidget::updateYAxisInfoLabel - Data source registry is null" << std::endl;
        return;
    }

    QString selected_key = getSelectedYAxisDataSource();
    if (selected_key.isEmpty()) {
        ui->y_axis_info_label->setText("Select a data source for the Y-axis");
        return;
    }

    QString info_text;
    
    if (selected_key.startsWith("analog:")) {
        QString analog_key = selected_key.mid(7);
        info_text = QString("Y-axis: Analog Time Series\nKey: %1").arg(analog_key);

        auto data_manager_source = _data_source_registry->getDataSource("primary_data_manager");
        auto data_manager = data_manager_source ? dynamic_cast<DataManagerSource*>(data_manager_source)->getDataManager() : nullptr;

        if (data_manager) {
            try {
                auto analog_data = data_manager->getData<AnalogTimeSeries>(analog_key.toStdString());
                if (analog_data) {
                    info_text += QString("\nSamples: %1").arg(analog_data->getNumSamples());
                }
            } catch (std::exception const & e) {
                info_text += QString("\nError: %1").arg(e.what());
            }
        }
    } else if (selected_key.startsWith("table:")) {
        QStringList parts = selected_key.split(":");
        if (parts.size() >= 3) {
            QString table_id = parts[1];
            QString column_name = parts[2];
            info_text = QString("Y-axis: Table Column\nTable: %1\nColumn: %2").arg(table_id, column_name);
        }
    } else {
        info_text = QString("Y-axis: Unknown data type\nKey: %1").arg(selected_key);
    }

    ui->y_axis_info_label->setText(info_text);
}

QStringList ScatterPlotPropertiesWidget::getAvailableNumericColumns() const {
    QStringList numeric_columns;
    
    if (!_data_source_registry) {
        std::cout << "ScatterPlotPropertiesWidget::getAvailableNumericColumns - Data source registry is null" << std::endl;
        return numeric_columns;
    }
    
    // Find TableManagerSource
    AbstractDataSource* table_manager_source = nullptr;
    auto source_ids = _data_source_registry->getAvailableSourceIds();

    for (auto const & source_id : source_ids) {
        auto* source = _data_source_registry->getDataSource(source_id);
        if (source && source->getType() == "TableManager") {
            table_manager_source = source;
            break;
        }
    }
    
    if (table_manager_source) {
        TableManagerSource* tm_source = static_cast<TableManagerSource*>(table_manager_source);
        auto table_manager = tm_source->getTableManager();
        
        if (table_manager) {
            auto table_ids = table_manager->getTableIds();
            
            for (auto const & table_id : table_ids) {
                auto table_view = table_manager->getBuiltTable(table_id);
                if (table_view) {
                    auto column_names = table_view->getColumnNames();
                    
                    for (auto const & column_name : column_names) {
                        // Use our type-safe interface to check if column is numeric
                        ColumnTypeInfo type_info = tm_source->getColumnTypeInfo(table_id, QString::fromStdString(column_name));
                        
                        // Check if it's a numeric vector type suitable for plotting
                        if (type_info.isVectorType && 
                            (type_info.hasElementType<float>() || 
                             type_info.hasElementType<double>() || 
                             type_info.hasElementType<int>())) {
                            
                            QString data_key = QString("table:%1:%2")
                                               .arg(table_id)
                                               .arg(QString::fromStdString(column_name));
                            numeric_columns.append(data_key);
                        }
                    }
                }
            }
        }
    }
    
    return numeric_columns;
}

QMap<QString, QString> ScatterPlotPropertiesWidget::getAvailableNumericColumnsWithTypes() const {
    QMap<QString, QString> columns_with_types;
    
    if (!_data_source_registry) {
        std::cout << "ScatterPlotPropertiesWidget::getAvailableNumericColumnsWithTypes - Data source registry is null" << std::endl;
        return columns_with_types;
    }
    
    // Find TableManagerSource
    AbstractDataSource* table_manager_source = nullptr;
    auto source_ids = _data_source_registry->getAvailableSourceIds();

    for (auto const & source_id : source_ids) {
        auto* source = _data_source_registry->getDataSource(source_id);
        if (source && source->getType() == "TableManager") {
            table_manager_source = source;
            break;
        }
    }
    
    if (table_manager_source) {
        TableManagerSource* tm_source = static_cast<TableManagerSource*>(table_manager_source);
        auto table_manager = tm_source->getTableManager();
        
        if (table_manager) {
            auto table_ids = table_manager->getTableIds();
            
            for (auto const & table_id : table_ids) {
                auto table_view = table_manager->getBuiltTable(table_id);
                if (table_view) {
                    auto column_names = table_view->getColumnNames();
                    
                    for (auto const & column_name : column_names) {
                        // Use our type-safe interface to get detailed type information
                        ColumnTypeInfo type_info = tm_source->getColumnTypeInfo(table_id, QString::fromStdString(column_name));
                        
                        // Check if it's a numeric vector type suitable for plotting
                        if (type_info.isVectorType && 
                            (type_info.hasElementType<float>() || 
                             type_info.hasElementType<double>() || 
                             type_info.hasElementType<int>())) {
                            
                            QString data_key = QString("table:%1:%2")
                                               .arg(table_id)
                                               .arg(QString::fromStdString(column_name));
                            
                            QString type_description = QString("%1 (element type: %2)")
                                                      .arg(QString::fromStdString(type_info.typeName))
                                                      .arg(QString::fromStdString(type_info.elementTypeName));
                            
                            columns_with_types[data_key] = type_description;
                        }
                    }
                }
            }
        }
    }
    
    return columns_with_types;
}

QStringList ScatterPlotPropertiesWidget::getNumericColumnsFromRegistry(DataSourceRegistry * data_source_registry) {
    QStringList numeric_columns;
    
    if (!data_source_registry) {
        std::cout << "ScatterPlotPropertiesWidget::getNumericColumnsFromRegistry - Data source registry is null" << std::endl;
        return numeric_columns;
    }
    
    // Find TableManagerSource
    AbstractDataSource* table_manager_source = nullptr;
    auto source_ids = data_source_registry->getAvailableSourceIds();

    for (auto const & source_id : source_ids) {
        auto* source = data_source_registry->getDataSource(source_id);
        if (source && source->getType() == "TableManager") {
            table_manager_source = source;
            break;
        }
    }
    
    if (table_manager_source) {
        TableManagerSource* tm_source = static_cast<TableManagerSource*>(table_manager_source);
        auto table_manager = tm_source->getTableManager();
        
        if (table_manager) {
            auto table_ids = table_manager->getTableIds();
            
            for (auto const & table_id : table_ids) {
                auto table_view = table_manager->getBuiltTable(table_id);
                if (table_view) {
                    auto column_names = table_view->getColumnNames();
                    
                    for (auto const & column_name : column_names) {
                        // Use type-safe interface to filter numeric columns
                        ColumnTypeInfo type_info = tm_source->getColumnTypeInfo(table_id, QString::fromStdString(column_name));
                        
                        // Only include numeric vector types
                        if (type_info.isVectorType && 
                            (type_info.hasElementType<float>() || 
                             type_info.hasElementType<double>() || 
                             type_info.hasElementType<int>())) {
                            
                            QString data_key = QString("table:%1:%2")
                                               .arg(table_id)
                                               .arg(QString::fromStdString(column_name));
                            numeric_columns.append(data_key);
                        }
                    }
                }
            }
        }
    }
    
    return numeric_columns;
}