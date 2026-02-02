#ifndef BINARY_DIGITAL_INTERVAL_IMPORT_WIDGET_HPP
#define BINARY_DIGITAL_INTERVAL_IMPORT_WIDGET_HPP

/**
 * @file BinaryDigitalIntervalImport_Widget.hpp
 * @brief Widget for configuring binary digital interval data import options
 */

#include <QWidget>
#include <QString>
#include "DataManager/IO/formats/Binary/digitaltimeseries/Digital_Interval_Series_Binary.hpp"

namespace Ui {
class BinaryDigitalIntervalImport_Widget;
}

/**
 * @brief Widget for binary digital interval import configuration
 */
class BinaryDigitalIntervalImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit BinaryDigitalIntervalImport_Widget(QWidget * parent = nullptr);
    ~BinaryDigitalIntervalImport_Widget() override;

signals:
    void loadBinaryIntervalRequested(BinaryIntervalLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onDataTypeChanged(int index);

private:
    Ui::BinaryDigitalIntervalImport_Widget * ui;
    void _updateChannelRange();
};

#endif // BINARY_DIGITAL_INTERVAL_IMPORT_WIDGET_HPP
