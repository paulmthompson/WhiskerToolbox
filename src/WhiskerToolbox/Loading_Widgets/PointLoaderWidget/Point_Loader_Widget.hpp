#ifndef POINT_LOADER_WIDGET_HPP
#define POINT_LOADER_WIDGET_HPP

#include <QWidget>

#include <memory>


class DataManager;

namespace Ui {
class Point_Loader_Widget;
}

class Point_Loader_Widget : public QWidget
{
    Q_OBJECT
public:

    Point_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    ~Point_Loader_Widget();

private:
    Ui::Point_Loader_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _loadSingleKeypoint();
};

#endif // POINT_LOADER_WIDGET_HPP
