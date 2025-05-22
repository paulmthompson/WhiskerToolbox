#ifndef DIGITAL_INTERVAL_LOADER_WIDGET_HPP
#define DIGITAL_INTERVAL_LOADER_WIDGET_HPP

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

    //void _loadSingleHDF5Line(std::string filename, std::string line_suffix = "");

private slots:
    void _loadSingleInterval();
    //void _loadSingleHdf5Line();
    //void _loadMultiHdf5Line();
};


#endif// DIGITAL_INTERVAL_LOADER_WIDGET_HPP
