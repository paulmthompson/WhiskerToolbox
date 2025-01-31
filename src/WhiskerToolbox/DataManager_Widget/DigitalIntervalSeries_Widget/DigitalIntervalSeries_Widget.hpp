#ifndef DIGITALINTERVALSERIES_WIDGET_HPP
#define DIGITALINTERVALSERIES_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

namespace Ui { class DigitalIntervalSeries_Widget; }

class DataManager;
class IntervalTableModel;

class DigitalIntervalSeries_Widget : public QWidget
{
    Q_OBJECT
public:
    explicit DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = nullptr);
    ~DigitalIntervalSeries_Widget();

    void openWidget(); // Call to open the widget

    void setActiveKey(std::string key);
signals:
    void frameSelected(int frame_id);

private:
    Ui::DigitalIntervalSeries_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;
    int _callback_id;
    bool _interval_epoch {false};
    int _interval_start {0};
    IntervalTableModel* _interval_table_model;


    void _calculateIntervals();
private slots:
    void _saveCSV();
    void _createIntervalButton();
    void _removeIntervalButton();
    void _flipIntervalButton();
    void _handleCellClicked(const QModelIndex &index);
};

#endif // DIGITALINTERVALSERIES_WIDGET_HPP
