#ifndef ANALOG_IMPORT_WIDGET_HPP
#define ANALOG_IMPORT_WIDGET_HPP

/**
 * @file AnalogImport_Widget.hpp
 * @brief Widget for importing analog time series data into DataManager
 * 
 * This widget provides a unified interface for importing analog time series data
 * from various formats (CSV, Binary). It uses a stacked widget to switch between
 * format-specific import widgets and handles the actual data loading into DataManager.
 */

#include "AnalogTimeSeries/CSV/CSVAnalogImport_Widget.hpp"
#include "AnalogTimeSeries/Binary/BinaryAnalogImport_Widget.hpp"

#include <QWidget>

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class AnalogImport_Widget;
}

/**
 * @brief Widget for importing analog time series data
 * 
 * Provides format selection (CSV, Binary) and delegates to format-specific
 * import widgets. Handles loading data into DataManager upon successful import.
 */
class AnalogImport_Widget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the analog import widget
     * @param data_manager Shared pointer to DataManager for data storage
     * @param parent Parent widget
     */
    explicit AnalogImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~AnalogImport_Widget() override;

signals:
    /**
     * @brief Emitted when data import completes successfully
     * @param data_key The key under which data was stored in DataManager
     * @param data_type The type of data that was imported
     */
    void importCompleted(QString const & data_key, QString const & data_type);

private:
    Ui::AnalogImport_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _onLoaderTypeChanged(int index);
    void _handleCSVLoadRequested(CSVAnalogLoaderOptions options);
    void _handleBinaryLoadRequested(BinaryAnalogLoaderOptions options);
};

#endif // ANALOG_IMPORT_WIDGET_HPP
