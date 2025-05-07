#ifndef DIGITALEVENTSERIES_WIDGET_HPP
#define DIGITALEVENTSERIES_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class DigitalEventSeries_Widget;
}

class DataManager;
class EventTableModel;

class DigitalEventSeries_Widget : public QWidget {
    Q_OBJECT
public:
    explicit DigitalEventSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~DigitalEventSeries_Widget() override;

    void openWidget();// Call to open the widget

    void setActiveKey(std::string key);
    void removeCallbacks();

signals:
    void frameSelected(int frame_id);

private:
    Ui::DigitalEventSeries_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;
    int _callback_id{0};
    EventTableModel * _event_table_model;

    void _calculateEvents();
    void _assignCallbacks();

private slots:
    //void _saveCSV();
    void _addEventButton();
    void _removeEventButton();
    void _handleCellClicked(QModelIndex const & index);
    void _changeDataTable(QModelIndex const & topLeft, QModelIndex const & bottomRight, QVector<int> const & roles);
};


#endif// DIGITALEVENTSERIES_WIDGET_HPP
