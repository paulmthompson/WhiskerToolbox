#ifndef CSV_ANALOG_IMPORT_WIDGET_HPP
#define CSV_ANALOG_IMPORT_WIDGET_HPP

/**
 * @file CSVAnalogImport_Widget.hpp
 * @brief Widget for configuring CSV analog time series data import options
 */

#include <QWidget>
#include <QString>
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"

namespace Ui {
class CSVAnalogImport_Widget;
}

/**
 * @brief Widget for CSV analog time series import configuration
 * 
 * Provides UI controls for configuring CSV analog data loading options:
 * - File selection
 * - Delimiter configuration
 * - Header row handling
 * - Column selection for time and data values
 * - Single column vs two column format
 */
class CSVAnalogImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVAnalogImport_Widget(QWidget * parent = nullptr);
    ~CSVAnalogImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load analog data from CSV
     * @param options The configured loader options
     */
    void loadAnalogCSVRequested(CSVAnalogLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onFormatChanged(int index);

private:
    Ui::CSVAnalogImport_Widget * ui;
    void _updateColumnVisibility();
};

#endif // CSV_ANALOG_IMPORT_WIDGET_HPP
