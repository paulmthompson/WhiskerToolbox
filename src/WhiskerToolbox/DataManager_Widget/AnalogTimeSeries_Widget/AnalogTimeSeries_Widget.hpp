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

    void setActiveKey(std::string key);

private:
    Ui::AnalogTimeSeries_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;

private slots:
    void _saveCSV();
};

#endif // ANALOGTIMESERIES_WIDGET_HPP
