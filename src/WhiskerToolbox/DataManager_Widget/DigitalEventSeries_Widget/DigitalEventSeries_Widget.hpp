#ifndef DIGITALEVENTSERIES_WIDGET_HPP
#define DIGITALEVENTSERIES_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui { class DigitalEventSeries_Widget; }

class DataManager;

class DigitalEventSeries_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit DigitalEventSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~DigitalEventSeries_Widget();

    void openWidget(); // Call to open the widget

private:
    Ui::DigitalEventSeries_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
               // Add any slots needed for handling user interactions
};

#endif // DIGITALEVENTSERIES_WIDGET_HPP
