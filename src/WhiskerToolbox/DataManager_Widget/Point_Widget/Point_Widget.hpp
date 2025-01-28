#ifndef POINT_WIDGET_HPP
#define POINT_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui { class Point_Widget; }

class DataManager;

class Point_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~Point_Widget();

    void openWidget(); // Call to open the widget

private:
    Ui::Point_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
               // Add any slots needed for handling user interactions
};

#endif // POINT_WIDGET_HPP
