#ifndef EXPORT_VIDEO_WIDGET_HPP
#define EXPORT_VIDEO_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class Export_Video_Widget;
}

class Export_Video_Widget : public QWidget
{
    Q_OBJECT
public:

    Export_Video_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    ~Export_Video_Widget();

    void openWidget();

private:
    Ui::Export_Video_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:

};



#endif // EXPORT_VIDEO_WIDGET_HPP
