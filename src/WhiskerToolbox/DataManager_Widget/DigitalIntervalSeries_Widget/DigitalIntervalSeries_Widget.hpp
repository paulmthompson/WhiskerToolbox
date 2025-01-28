#ifndef DIGITALINTERVALSERIES_WIDGET_HPP
#define DIGITALINTERVALSERIES_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui { class DigitalIntervalSeries_Widget; }

class DataManager;

class DigitalIntervalSeries_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~DigitalIntervalSeries_Widget();

    void openWidget(); // Call to open the widget

private:
    Ui::DigitalIntervalSeries_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
               // Add any slots needed for handling user interactions
};

#endif // DIGITALINTERVALSERIES_WIDGET_HPP
