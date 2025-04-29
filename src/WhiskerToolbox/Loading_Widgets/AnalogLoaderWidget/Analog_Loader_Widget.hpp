#ifndef ANALOG_LOADER_WIDGET_HPP
#define ANALOG_LOADER_WIDGET_HPP

#include <QWidget>

#include <memory>

class DataManager;

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
    void _loadAnalogCSV();
};


#endif// ANALOG_LOADER_WIDGET_HPP
