#ifndef CSV_DIGITAL_INTERVAL_IMPORT_WIDGET_HPP
#define CSV_DIGITAL_INTERVAL_IMPORT_WIDGET_HPP

/**
 * @file CSVDigitalIntervalImport_Widget.hpp
 * @brief Widget for configuring CSV digital interval data import options
 */

#include <QWidget>
#include <QString>
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"

namespace Ui {
class CSVDigitalIntervalImport_Widget;
}

/**
 * @brief Widget for CSV digital interval import configuration
 */
class CSVDigitalIntervalImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVDigitalIntervalImport_Widget(QWidget * parent = nullptr);
    ~CSVDigitalIntervalImport_Widget() override;

signals:
    void loadCSVIntervalRequested(CSVIntervalLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();

private:
    Ui::CSVDigitalIntervalImport_Widget * ui;
};

#endif // CSV_DIGITAL_INTERVAL_IMPORT_WIDGET_HPP
