#ifndef DIGITALINTERVALSERIES_WIDGET_HPP
#define DIGITALINTERVALSERIES_WIDGET_HPP

#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"// For CSVIntervalSaverOptions

#include <QMenu>
#include <QWidget>

#include <memory>
#include <string>
#include <variant>// std::variant


namespace Ui {
class DigitalIntervalSeries_Widget;
}
class DataManager;
class IntervalTableModel;
class CSVIntervalSaver_Widget;


using IntervalSaverOptionsVariant = std::variant<CSVIntervalSaverOptions>;// Will expand if more types are added

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
    int _callback_id{-1};// Corrected initialization
    bool _interval_epoch{false};
    int _interval_start{0};
    IntervalTableModel * _interval_table_model;

    enum SaverType { CSV };// Enum for different saver types

    void _calculateIntervals();
    void _assignCallbacks();

    void _initiateSaveProcess(SaverType saver_type, IntervalSaverOptionsVariant & options_variant);
    bool _performActualCSVSave(CSVIntervalSaverOptions & options);


    void _populateMoveToComboBox();
    std::vector<Interval> _getSelectedIntervals();
    void _showContextMenu(QPoint const & position);

private slots:

    void _createIntervalButton();
    void _removeIntervalButton();
    void _flipIntervalButton();
    void _handleCellClicked(QModelIndex const & index);
    void _changeDataTable(QModelIndex const & topLeft, QModelIndex const & bottomRight, QVector<int> const & roles = QVector<int>());// Added default for roles
    void _extendInterval();

    void _onExportTypeChanged(int index);
    void _handleSaveIntervalCSVRequested(CSVIntervalSaverOptions options);

    void _moveIntervalsButton();
    void _copyIntervalsButton();
    void _mergeIntervalsButton();
};

#endif// DIGITALINTERVALSERIES_WIDGET_HPP
