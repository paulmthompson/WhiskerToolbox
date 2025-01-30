#ifndef ANALOGTIMESERIES_WIDGET_HPP
#define ANALOGTIMESERIES_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui { class AnalogTimeSeries_Widget; }

class DataManager;

class AnalogTimeSeries_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit AnalogTimeSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~AnalogTimeSeries_Widget();

    void openWidget(); // Call to open the widget

private:
    Ui::AnalogTimeSeries_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
               // Add any slots needed for handling user interactions
};

#endif // ANALOGTIMESERIES_WIDGET_HPP
