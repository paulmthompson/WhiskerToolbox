#ifndef DIGITALINTERVALSERIES_WIDGET_HPP
#define DIGITALINTERVALSERIES_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class DigitalIntervalSeries_Widget;
}

class DataManager;
class IntervalTableModel;

class DigitalIntervalSeries_Widget : public QWidget {
    Q_OBJECT
public:
    explicit DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~DigitalIntervalSeries_Widget() override;

    void openWidget();// Call to open the widget

    void setActiveKey(std::string key);
    void removeCallbacks();
signals:
    void frameSelected(int frame_id);

private:
    Ui::DigitalIntervalSeries_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;
    int _callback_id{0};
    bool _interval_epoch{false};
    int _interval_start{0};
    IntervalTableModel * _interval_table_model;


    void _calculateIntervals();
    void _assignCallbacks();

private slots:
    void _saveCSV();
    void _createIntervalButton();
    void _removeIntervalButton();
    void _flipIntervalButton();
    void _handleCellClicked(QModelIndex const & index);
    void _changeDataTable(QModelIndex const & topLeft, QModelIndex const & bottomRight, QVector<int> const & roles);
    void _extendInterval();
};

#endif// DIGITALINTERVALSERIES_WIDGET_HPP
