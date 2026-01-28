#ifndef DIGITAL_INTERVAL_IMPORT_WIDGET_HPP
#define DIGITAL_INTERVAL_IMPORT_WIDGET_HPP

/**
 * @file DigitalIntervalImport_Widget.hpp
 * @brief Widget for importing digital interval series data into DataManager
 */

#include "DigitalTimeSeries/CSV/CSVDigitalIntervalImport_Widget.hpp"
#include "DigitalTimeSeries/Binary/BinaryDigitalIntervalImport_Widget.hpp"

#include <QWidget>

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class DigitalIntervalImport_Widget;
}

/**
 * @brief Widget for importing digital interval series data
 */
class DigitalIntervalImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit DigitalIntervalImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~DigitalIntervalImport_Widget() override;

signals:
    void importCompleted(QString const & data_key, QString const & data_type);

private:
    Ui::DigitalIntervalImport_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _onLoaderTypeChanged(int index);
    void _handleCSVLoadRequested(CSVIntervalLoaderOptions options);
    void _handleBinaryLoadRequested(BinaryIntervalLoaderOptions options);
};

#endif // DIGITAL_INTERVAL_IMPORT_WIDGET_HPP
