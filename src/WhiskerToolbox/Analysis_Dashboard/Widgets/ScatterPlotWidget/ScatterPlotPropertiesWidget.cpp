
#include "ScatterPlotPropertiesWidget.hpp"
#include "ScatterPlotWidget.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/TableView/TableRegistry.hpp"
#include "DataManager/utils/TableView/columns/ColumnTypeInfo.hpp"
#include "DataManager/utils/TableView/core/TableView.h"
#include "ui_ScatterPlotPropertiesWidget.h"
#include "Analysis_Dashboard/DataView/Transforms/FilterByRangeTransform.hpp"
#include "Analysis_Dashboard/DataView/Transforms/ColorByFeatureTransform.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QPushButton>

#include <memory>
#include <numeric>
#include <vector>

ScatterPlotPropertiesWidget::ScatterPlotPropertiesWidget(QWidget * parent)
    : AbstractPlotPropertiesWidget(parent),
      ui(new Ui::ScatterPlotPropertiesWidget),
      _scatter_plot_widget(nullptr),
      _applying_properties(false) {
    ui->setupUi(this);
    setupConnections();
    setViewControlsHelp();
}

ScatterPlotPropertiesWidget::~ScatterPlotPropertiesWidget() {
    delete ui;
}

void ScatterPlotPropertiesWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    updateAvailableDataSources();
}

void ScatterPlotPropertiesWidget::setPlotWidget(AbstractPlotWidget * plot_widget) {
    _plot_widget = plot_widget;
    _scatter_plot_widget = qobject_cast<ScatterPlotWidget *>(plot_widget);

    if (_scatter_plot_widget) {
        updateFromPlot();
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

    // Update coordinate range display
    updateCoordinateRange();
}

void ScatterPlotPropertiesWidget::applyToPlot() {
    if (!_scatter_plot_widget) {
        return;
    }

    _applying_properties = true;
    updatePlotWidget();
    _applying_properties = false;
}

void ScatterPlotPropertiesWidget::rebuildPipeline() {
    _pipeline.clear();
    // TODO: Add UI controls similar to EventPlot for filter/color and build transforms here
}

void ScatterPlotPropertiesWidget::updateAvailableDataSources() {
    if (!ui->x_axis_combo || !ui->y_axis_combo) {
        return;
    }

    if (!_data_manager) {
        std::cout << "ScatterPlotPropertiesWidget::updateAvailableDataSources - DataManager is null" << std::endl;
        return;
    }

    qDebug() << "ScatterPlotPropertiesWidget::updateAvailableDataSources: Starting update";

    // Clear existing items
    ui->x_axis_combo->clear();
    ui->y_axis_combo->clear();

    ui->x_axis_combo->addItem("Select a data source...", "");
    ui->y_axis_combo->addItem("Select a data source...", "");

    // Add items from DataManager (analog time series)
    std::vector<std::string> all_keys = _data_manager->getAllKeys();

    for (std::string const & key: all_keys) {
        DM_DataType data_type = _data_manager->getType(key);

        // Only add analog time series for scatter plots
        if (data_type == DM_DataType::Analog) {
            QString display_text = QString("Analog: %1").arg(QString::fromStdString(key));
            QString data_key = QString("analog:%1").arg(QString::fromStdString(key));

            ui->x_axis_combo->addItem(display_text, data_key);
            ui->y_axis_combo->addItem(display_text, data_key);

            qDebug() << "Added analog time series:" << display_text;
        }
    }

    // Add items from TableRegistry (table columns)
    if (auto * registry = _data_manager->getTableRegistry()) {
        auto table_ids = registry->getTableIds();
        qDebug() << "Found" << table_ids.size() << "tables in table manager";

        for (auto const & table_id: table_ids) {
            qDebug() << "Processing table:" << table_id;

            auto table_view = registry->getBuiltTable(table_id);
            if (table_view) {
                qDebug() << "Table" << table_id << "is built and available";
                auto column_names = table_view->getColumnNames();
                qDebug() << "Table" << table_id << "has" << column_names.size() << "columns:" << QString::fromStdString(std::accumulate(column_names.begin(), column_names.end(), std::string{}, [](std::string const & a, std::string const & b) { return a.empty() ? b : a + ", " + b; }));

                for (auto const & column_name: column_names) {
                    // Use our new type-safe interface to filter by numeric types
                    // Inspect type via TableView runtime type
                    ColumnTypeInfo type_info;
                    try {
                        auto type_index = table_view->getColumnTypeIndex(column_name);
                        if (type_index == typeid(std::vector<float>)) type_info = ColumnTypeInfo::fromType<std::vector<float>>();
                        else if (type_index == typeid(std::vector<double>))
                            type_info = ColumnTypeInfo::fromType<std::vector<double>>();
                        else if (type_index == typeid(std::vector<int>))
                            type_info = ColumnTypeInfo::fromType<std::vector<int>>();
                        else if (type_index == typeid(float))
                            type_info = ColumnTypeInfo(typeid(float), typeid(float), false, false, "float", "float");
                        else if (type_index == typeid(double))
                            type_info = ColumnTypeInfo(typeid(double), typeid(double), false, false, "double", "double");
                        else if (type_index == typeid(int))
                            type_info = ColumnTypeInfo(typeid(int), typeid(int), false, false, "int", "int");
                        else
                            type_info = ColumnTypeInfo{};
                    } catch (...) { type_info = ColumnTypeInfo{}; }

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
                                                       .arg(QString::fromStdString(table_id))
                                                       .arg(QString::fromStdString(column_name))
                                                       .arg(type_display);
                        QString data_key = QString("table:%1:%2")
                                                   .arg(QString::fromStdString(table_id))
                                                   .arg(QString::fromStdString(column_name));

                        ui->x_axis_combo->addItem(display_text, data_key);
                        ui->y_axis_combo->addItem(display_text, data_key);

                        qDebug() << "Added numeric table column:" << display_text
                                 << "Type:" << QString::fromStdString(type_info.typeName);
                    } else {
                        qDebug() << "Skipped non-numeric column:" << QString::fromStdString(column_name)
                                 << "Type:" << QString::fromStdString(type_info.typeName)
                                 << "Reason: isVectorType=" << type_info.isVectorType
                                 << "isNumeric=" << (type_info.hasElementType<float>() || type_info.hasElementType<double>() || type_info.hasElementType<int>());
                    }
                }
            } else {
                qDebug() << "Table" << table_id << "is not built yet";
            }
        }
    } else {
        qDebug() << "TableManagerSource has no DataManager";
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

    // Store data for coordinate range calculations
    _x_data = x_data;
    _y_data = y_data;

    // Make sure both data vectors have the same size
    size_t min_size = std::min(x_data.size(), y_data.size());
    if (x_data.size() != y_data.size()) {
        qDebug() << "Data size mismatch, trimming to minimum size:" << min_size;
        x_data.resize(min_size);
        y_data.resize(min_size);
        _x_data.resize(min_size);
        _y_data.resize(min_size);
    }

    // Apply pipeline row-level mask and color (if configured)
    if (_data_manager) {
        auto * registry = _data_manager->getTableRegistry();
        if (registry) {
            DataViewContext ctx;
            ctx.tableId = ""; // not table-backed by default; pipeline can still color via selected table
            ctx.tableView = nullptr;
            ctx.tableRegistry = registry;
            ctx.rowCount = std::min(x_data.size(), y_data.size());
            auto state = _pipeline.evaluate(ctx);
            if (!state.rowMask.empty()) {
                std::vector<float> xf, yf;
                xf.reserve(ctx.rowCount);
                yf.reserve(ctx.rowCount);
                for (size_t i = 0; i < ctx.rowCount; ++i) {
                    if (i < state.rowMask.size() ? state.rowMask[i] != 0 : true) {
                        xf.push_back(x_data[i]);
                        yf.push_back(y_data[i]);
                    }
                }
                x_data.swap(xf);
                y_data.swap(yf);
            }
            // TODO: propagate state.rowColorIndices to Scatter visualization when per-point API is exposed
        }
    }

    // Update the scatter plot widget if available
    if (_scatter_plot_widget) {
        _scatter_plot_widget->setScatterData(x_data, y_data);

        // Set axis labels based on data keys
        QString x_label = x_data_key.startsWith("analog:") ? x_data_key.mid(7) : (x_data_key.startsWith("table:") ? x_data_key.split(":").last() : x_data_key);
        QString y_label = y_data_key.startsWith("analog:") ? y_data_key.mid(7) : (y_data_key.startsWith("table:") ? y_data_key.split(":").last() : y_data_key);

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

    if (!_data_manager) {
        std::cout << "ScatterPlotPropertiesWidget::loadDataFromKey - DataManager is null" << std::endl;
        return result;
    }

    if (data_key.startsWith("analog:")) {
        // Handle analog time series
        QString analog_key = data_key.mid(7);// Remove "analog:" prefix

        try {
            auto analog_data = _data_manager->getData<AnalogTimeSeries>(analog_key.toStdString());
            if (analog_data) {
                auto data_vector = analog_data->getAnalogTimeSeries();
                result.reserve(data_vector.size());

                for (auto value: data_vector) {
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

            if (auto * registry = _data_manager->getTableRegistry()) {
                try {
                    // Use TableView's variant accessor directly
                    auto view = registry->getBuiltTable(table_id.toStdString());
                    if (view) {
                        auto column_variant = view->getColumnDataVariant(column_name.toStdString());

                        // Create a visitor to extract numeric data as float vector
                        auto float_vector_visitor = [&result](auto const & data) {
                            using DataType = std::decay_t<decltype(data)>;

                            if constexpr (std::is_same_v<DataType, std::vector<float>>) {
                                result = data;
                            } else if constexpr (std::is_same_v<DataType, std::vector<double>>) {
                                result.reserve(data.size());
                                for (auto const & value: data) {
                                    result.push_back(static_cast<float>(value));
                                }
                            } else if constexpr (std::is_same_v<DataType, std::vector<int>>) {
                                result.reserve(data.size());
                                for (auto const & value: data) {
                                    result.push_back(static_cast<float>(value));
                                }
                            } else if constexpr (std::is_same_v<DataType, float>) {
                                result.push_back(data);
                            } else if constexpr (std::is_same_v<DataType, double>) {
                                result.push_back(static_cast<float>(data));
                            } else if constexpr (std::is_same_v<DataType, int>) {
                                result.push_back(static_cast<float>(data));
                            } else {
                                qWarning() << "Unsupported column data type for scatter plot";
                            }
                        };

                        std::visit(float_vector_visitor, column_variant);
                    }

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

    if (!_data_manager) {
        std::cout << "ScatterPlotPropertiesWidget::updateXAxisInfoLabel - DataManager is null" << std::endl;
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

        if (_data_manager) {
            try {
                auto analog_data = _data_manager->getData<AnalogTimeSeries>(analog_key.toStdString());
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

    if (!_data_manager) {
        std::cout << "ScatterPlotPropertiesWidget::updateYAxisInfoLabel - DataManager is null" << std::endl;
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

        if (_data_manager) {
            try {
                auto analog_data = _data_manager->getData<AnalogTimeSeries>(analog_key.toStdString());
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

    if (!_data_manager) {
        std::cout << "ScatterPlotPropertiesWidget::getAvailableNumericColumns - DataManager is null" << std::endl;
        return numeric_columns;
    }

    auto * registry = _data_manager->getTableRegistry();
    if (!registry) return numeric_columns;

    auto table_ids = registry->getTableIds();

    auto isNumericIndex = [](std::type_index const & idx) -> bool {
        return idx == typeid(float) || idx == typeid(double) || idx == typeid(int) ||
               idx == typeid(std::vector<float>) || idx == typeid(std::vector<double>) || idx == typeid(std::vector<int>);
    };

    auto isNumericFromInfo = [](ColumnTypeInfo const & info) -> bool {
        if (info.isVectorType) {
            return info.hasElementType<float>() || info.hasElementType<double>() || info.hasElementType<int>();
        }
        return info.typeName == std::string("float") || info.typeName == std::string("double") || info.typeName == std::string("int");
    };

    for (auto const & table_id: table_ids) {
        auto table_view = registry->getBuiltTable(table_id);
        if (table_view) {
            auto column_names = table_view->getColumnNames();
            for (auto const & column_name: column_names) {
                try {
                    auto idx = table_view->getColumnTypeIndex(column_name);
                    if (isNumericIndex(idx)) {
                        QString data_key = QString("table:%1:%2").arg(QString::fromStdString(table_id)).arg(QString::fromStdString(column_name));
                        numeric_columns.append(data_key);
                    }
                } catch (...) {
                    // ignore columns we cannot type
                }
            }
            continue;
        }

        // Not built: fall back to definition
        auto info = registry->getTableInfo(table_id);
        for (auto const & col : info.columns) {
            if (isNumericFromInfo(col.typeInfo)) {
                QString data_key = QString("table:%1:%2").arg(QString::fromStdString(table_id)).arg(QString::fromStdString(col.name));
                numeric_columns.append(data_key);
            }
        }
    }

    return numeric_columns;
}

QMap<QString, QString> ScatterPlotPropertiesWidget::getAvailableNumericColumnsWithTypes() const {
    QMap<QString, QString> columns_with_types;

    if (!_data_manager) {
        std::cout << "ScatterPlotPropertiesWidget::getAvailableNumericColumnsWithTypes - DataManager is null" << std::endl;
        return columns_with_types;
    }

    auto * registry = _data_manager->getTableRegistry();
    if (!registry) return columns_with_types;

    auto table_ids = registry->getTableIds();

    auto describeIndex = [](std::type_index const & idx) -> QString {
        if (idx == typeid(float)) return "float";
        if (idx == typeid(double)) return "double";
        if (idx == typeid(int)) return "int";
        if (idx == typeid(std::vector<float>)) return "std::vector<float>";
        if (idx == typeid(std::vector<double>)) return "std::vector<double>";
        if (idx == typeid(std::vector<int>)) return "std::vector<int>";
        return "unknown";
    };

    auto isNumericIndex = [&](std::type_index const & idx) -> bool {
        return describeIndex(idx) != "unknown";
    };

    auto isNumericFromInfo = [](ColumnTypeInfo const & info) -> bool {
        if (info.isVectorType) {
            return info.hasElementType<float>() || info.hasElementType<double>() || info.hasElementType<int>();
        }
        return info.typeName == std::string("float") || info.typeName == std::string("double") || info.typeName == std::string("int");
    };

    for (auto const & table_id: table_ids) {
        auto table_view = registry->getBuiltTable(table_id);
        if (table_view) {
            auto column_names = table_view->getColumnNames();
            for (auto const & column_name: column_names) {
                try {
                    auto idx = table_view->getColumnTypeIndex(column_name);
                    if (isNumericIndex(idx)) {
                        QString key = QString("table:%1:%2").arg(QString::fromStdString(table_id)).arg(QString::fromStdString(column_name));
                        columns_with_types[key] = describeIndex(idx);
                    }
                } catch (...) {
                }
            }
            continue;
        }

        // Not built: use definition
        auto info = registry->getTableInfo(table_id);
        for (auto const & col : info.columns) {
            if (isNumericFromInfo(col.typeInfo)) {
                QString key = QString("table:%1:%2").arg(QString::fromStdString(table_id)).arg(QString::fromStdString(col.name));
                QString desc;
                if (col.typeInfo.isVectorType) {
                    desc = QString("std::vector<%1>").arg(QString::fromStdString(col.typeInfo.elementTypeName));
                } else {
                    desc = QString::fromStdString(col.typeInfo.typeName);
                }
                columns_with_types[key] = desc;
            }
        }
    }
    return columns_with_types;
}

void ScatterPlotPropertiesWidget::updateCoordinateRange() {
    /*
    if (!_scatter_plot_widget || !_scatter_plot_widget->getOpenGLWidget()) {
        ui->x_range_value->setText("No data");
        ui->y_range_value->setText("No data");
        return;
    }

    auto opengl_widget = _scatter_plot_widget->getOpenGLWidget();
    float zoom_level = opengl_widget->getZoomLevel();
    QVector2D pan_offset = opengl_widget->getPanOffset();

    // Get the stored data for calculating bounds
    if (_x_data.empty() || _y_data.empty()) {
        ui->x_range_value->setText("No data");
        ui->y_range_value->setText("No data");
        return;
    }

    // Calculate data bounds
    float min_x = _x_data[0];
    float max_x = _x_data[0];
    float min_y = _y_data[0];
    float max_y = _y_data[0];

    for (size_t i = 1; i < _x_data.size(); ++i) {
        min_x = std::min(min_x, _x_data[i]);
        max_x = std::max(max_x, _x_data[i]);
        min_y = std::min(min_y, _y_data[i]);
        max_y = std::max(max_y, _y_data[i]);
    }

    // Calculate the visible range based on zoom and pan
    // The OpenGL widget uses a projection matrix that maps world coordinates to screen
    // We need to calculate what portion of the data is currently visible

    // Calculate data dimensions
    float data_width = max_x - min_x;
    float data_height = max_y - min_y;

    // Calculate the visible window based on zoom level
    // The projection matrix creates an orthographic view with padding
    float padding = 1.1f;// 10% padding (same as SpatialOverlayOpenGLWidget)
    float zoom_factor = 1.0f / zoom_level;
    float half_width = (data_width * padding * zoom_factor) / 2.0f;
    float half_height = (data_height * padding * zoom_factor) / 2.0f;

    // Calculate center of data
    float center_x = (min_x + max_x) / 2.0f;
    float center_y = (min_y + max_y) / 2.0f;

    // Apply pan offset (convert from normalized to world coordinates)
    float pan_x = pan_offset.x() * data_width * zoom_factor;
    float pan_y = pan_offset.y() * data_height * zoom_factor;

    // Calculate visible bounds
    float left_world = center_x - half_width + pan_x;
    float right_world = center_x + half_width + pan_x;
    float bottom_world = center_y - half_height + pan_y;
    float top_world = center_y + half_height + pan_y;

    // Format the range strings
    QString x_range_text = QString("[%1, %2]")
                                   .arg(left_world, 0, 'f', 3)
                                   .arg(right_world, 0, 'f', 3);
    QString y_range_text = QString("[%1, %2]")
                                   .arg(bottom_world, 0, 'f', 3)
                                   .arg(top_world, 0, 'f', 3);

    ui->x_range_value->setText(x_range_text);
    ui->y_range_value->setText(y_range_text);
    */
}

void ScatterPlotPropertiesWidget::onZoomLevelChanged(float zoom_level) {
    Q_UNUSED(zoom_level)
    updateCoordinateRange();
}

void ScatterPlotPropertiesWidget::onPanOffsetChanged(float offset_x, float offset_y) {
    Q_UNUSED(offset_x)
    Q_UNUSED(offset_y)
    updateCoordinateRange();
}

void ScatterPlotPropertiesWidget::setViewControlsHelp() {
    if (!ui->view_help_label) return;
    const QString text =
        "View Controls:\n"
        "- Mouse Wheel: Zoom (both axes)\n"
        "- Ctrl + Wheel: Zoom X only\n"
        "- Shift + Wheel: Zoom Y only\n"
        "- Left-Drag: Pan\n"
        "- Alt + Left-Drag: Box-zoom to rectangle";
    ui->view_help_label->setText(text);
}

