#ifndef ANALOG_LOADER_WIDGET_HPP
#define ANALOG_LOADER_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "DataManager/AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"

class DataManager;
class CSVAnalogLoader_Widget;
class BinaryAnalogLoader_Widget;

namespace Ui {
class Analog_Loader_Widget;
}

class Analog_Loader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Analog_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Analog_Loader_Widget() override;

private:
    Ui::Analog_Loader_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _onLoaderTypeChanged(int index);
    void _handleAnalogCSVLoadRequested(CSVAnalogLoaderOptions options);
    void _handleBinaryAnalogLoadRequested(BinaryAnalogLoaderOptions options);
};


#endif// ANALOG_LOADER_WIDGET_HPP
