#ifndef DIGITALINTERVALSERIES_WIDGET_HPP
#define DIGITALINTERVALSERIES_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>
#include <variant> // Required for std::variant

#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp" // For CSVIntervalSaverOptions

// Forward declarations of Qt classes and custom widgets
namespace Ui {
class DigitalIntervalSeries_Widget;
}
class DataManager;
class IntervalTableModel;
class CSVIntervalSaver_Widget; // Forward declare the new saver widget

// Define the variant type for saver options
using IntervalSaverOptionsVariant = std::variant<CSVIntervalSaverOptions>; // Will expand if more types are added

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
    int _callback_id{-1}; // Corrected initialization
    bool _interval_epoch{false};
    int _interval_start{0};
    IntervalTableModel * _interval_table_model;

    enum SaverType { CSV }; // Enum for different saver types

    void _calculateIntervals();
    void _assignCallbacks();

    // New helper methods for saving
    void _initiateSaveProcess(SaverType saver_type, IntervalSaverOptionsVariant& options_variant);
    bool _performActualCSVSave(CSVIntervalSaverOptions & options);

private slots:
    // Removed old _saveCSV slot
    void _createIntervalButton();
    void _removeIntervalButton();
    void _flipIntervalButton();
    void _handleCellClicked(QModelIndex const & index);
    void _changeDataTable(QModelIndex const & topLeft, QModelIndex const & bottomRight, const QVector<int> & roles = QVector<int> ()); // Added default for roles
    void _extendInterval();

    // New slots for saving
    void _onExportTypeChanged(int index);
    void _handleSaveIntervalCSVRequested(CSVIntervalSaverOptions options);

};

#endif// DIGITALINTERVALSERIES_WIDGET_HPP
