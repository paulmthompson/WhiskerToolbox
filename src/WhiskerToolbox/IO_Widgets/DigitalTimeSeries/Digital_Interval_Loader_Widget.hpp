#ifndef DIGITAL_INTERVAL_LOADER_WIDGET_HPP
#define DIGITAL_INTERVAL_LOADER_WIDGET_HPP

#include "DigitalTimeSeries/CSV/CSVDigitalIntervalLoader_Widget.hpp"
#include "DigitalTimeSeries/Binary/BinaryDigitalIntervalLoader_Widget.hpp"

#include <QWidget>

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class Digital_Interval_Loader_Widget;
}

class Digital_Interval_Loader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Digital_Interval_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Digital_Interval_Loader_Widget() override;

private:
    Ui::Digital_Interval_Loader_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _onLoaderTypeChanged(int index); 
    void _handleCSVLoadRequested(CSVIntervalLoaderOptions options);
    void _handleBinaryLoadRequested(BinaryIntervalLoaderOptions options); 
};


#endif// DIGITAL_INTERVAL_LOADER_WIDGET_HPP
