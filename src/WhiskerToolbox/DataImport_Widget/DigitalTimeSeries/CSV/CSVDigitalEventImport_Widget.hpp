#ifndef CSV_DIGITAL_EVENT_IMPORT_WIDGET_HPP
#define CSV_DIGITAL_EVENT_IMPORT_WIDGET_HPP

/**
 * @file CSVDigitalEventImport_Widget.hpp
 * @brief Widget for configuring CSV digital event data import options
 */

#include <QWidget>
#include <QString>
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"

namespace Ui {
class CSVDigitalEventImport_Widget;
}

/**
 * @brief Widget for CSV digital event import configuration
 * 
 * Allows users to configure:
 * - CSV file path
 * - Delimiter (comma, space, tab)
 * - Header row presence
 * - Event column index
 * - Optional identifier column for multiple event series
 */
class CSVDigitalEventImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVDigitalEventImport_Widget(QWidget * parent = nullptr);
    ~CSVDigitalEventImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load CSV event data
     * @param options Configured loader options
     */
    void loadCSVEventRequested(CSVEventLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();
    void _onIdentifierCheckboxToggled(bool checked);

private:
    Ui::CSVDigitalEventImport_Widget * ui;
    void _updateUIForIdentifierMode();
};

#endif // CSV_DIGITAL_EVENT_IMPORT_WIDGET_HPP
