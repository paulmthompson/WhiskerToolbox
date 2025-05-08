#ifndef POINT_WIDGET_HPP
#define POINT_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>

namespace Ui {
class Point_Widget;
}

class DataManager;
class PointTableModel;

class Point_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Point_Widget() override;

    void openWidget();// Call to open the widget

    void setActiveKey(std::string const & key);

    void updateTable();

    void loadFrame(int frame_id);

private:
    Ui::Point_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    PointTableModel * _point_table_model;
    std::string _active_key;
    int _previous_frame{0};

    //void refreshTable();
    void _propagateLabel(int frame_id);

private slots:
    void _saveKeypointCSV();
};

#endif// POINT_WIDGET_HPP
