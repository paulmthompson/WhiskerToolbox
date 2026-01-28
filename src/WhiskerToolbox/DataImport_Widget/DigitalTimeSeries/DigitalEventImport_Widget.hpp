#ifndef DIGITAL_EVENT_IMPORT_WIDGET_HPP
#define DIGITAL_EVENT_IMPORT_WIDGET_HPP

/**
 * @file DigitalEventImport_Widget.hpp
 * @brief Widget for importing digital event series data into DataManager
 * 
 * This widget provides a unified interface for loading digital event data
 * from various file formats. Currently supports CSV format.
 */

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QComboBox;
class QStackedWidget;
struct CSVEventLoaderOptions;
class DigitalEventSeries;

namespace Ui {
class DigitalEventImport_Widget;
}

/**
 * @brief Widget for importing digital event series data
 * 
 * Provides:
 * - Data name input
 * - Format selector (CSV)
 * - Format-specific options (via stacked widget)
 */
class DigitalEventImport_Widget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the DigitalEventImport_Widget
     * @param data_manager DataManager for storing imported data
     * @param parent Parent widget
     */
    explicit DigitalEventImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~DigitalEventImport_Widget() override;

signals:
    /**
     * @brief Emitted when digital event data import completes successfully
     * @param data_key Key of the imported data in DataManager
     * @param data_type Type string ("DigitalEventSeries")
     */
    void importCompleted(QString const & data_key, QString const & data_type);

private:
    Ui::DigitalEventImport_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    void _loadCSVEventData(std::vector<std::shared_ptr<DigitalEventSeries>> const & event_series_list,
                           CSVEventLoaderOptions const & options);

private slots:
    void _onLoaderTypeChanged(int index);
    void _handleLoadCSVEventRequested(CSVEventLoaderOptions options);
};

#endif // DIGITAL_EVENT_IMPORT_WIDGET_HPP
