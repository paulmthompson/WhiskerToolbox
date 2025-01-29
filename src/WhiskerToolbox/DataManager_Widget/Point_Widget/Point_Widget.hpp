#ifndef POINT_WIDGET_HPP
#define POINT_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui { class Point_Widget; }

class DataManager;
class PointTableModel;

class Point_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~Point_Widget();

    void openWidget(); // Call to open the widget

    void setActiveKey(const std::string &key);
    void assignPoint(qreal x,qreal y);

private:
    Ui::Point_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    PointTableModel* _point_table_model;
    std::string _active_key;

    enum Selection_Type {Point_Select};

    Point_Widget::Selection_Type _selection_mode {Point_Select};

    //void refreshTable();

private slots:
    void _saveKeypointCSV();

};

#endif // POINT_WIDGET_HPP
