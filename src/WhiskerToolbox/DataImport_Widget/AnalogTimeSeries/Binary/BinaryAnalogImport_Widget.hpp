#ifndef BINARY_ANALOG_IMPORT_WIDGET_HPP
#define BINARY_ANALOG_IMPORT_WIDGET_HPP

/**
 * @file BinaryAnalogImport_Widget.hpp
 * @brief Widget for configuring binary analog time series data import options
 */

#include <QWidget>
#include <QString>
#include "DataManager/AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"

namespace Ui {
class BinaryAnalogImport_Widget;
}

/**
 * @brief Widget for binary analog time series import configuration
 * 
 * Provides UI controls for configuring binary analog data loading options:
 * - File selection
 * - Header size configuration
 * - Number of channels
 * - Data type (int8, int16, uint16, float32, float64)
 * - Memory-mapped loading option
 * - Scale factor and offset for data conversion
 * - Sample offset and stride for channel extraction
 */
class BinaryAnalogImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit BinaryAnalogImport_Widget(QWidget * parent = nullptr);
    ~BinaryAnalogImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load analog data from binary file
     * @param options The configured loader options
     */
    void loadBinaryAnalogRequested(BinaryAnalogLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onDataTypeChanged(int index);

private:
    Ui::BinaryAnalogImport_Widget * ui;
    void _updateInfoLabel();
};

#endif // BINARY_ANALOG_IMPORT_WIDGET_HPP
