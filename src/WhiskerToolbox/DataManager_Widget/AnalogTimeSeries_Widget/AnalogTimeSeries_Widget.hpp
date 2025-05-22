#ifndef ANALOGTIMESERIES_WIDGET_HPP
#define ANALOGTIMESERIES_WIDGET_HPP

#include <QWidget>
#include <memory>
#include <string>
#include <variant> // Required for std::variant

// Forward declarations of Qt classes
class QStackedWidget;
class QComboBox;
class QLineEdit; // Added for filename_edit

#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp" // For CSVAnalogSaverOptions

namespace Ui {
class AnalogTimeSeries_Widget;
}

class DataManager;
class CSVAnalogSaver_Widget; // Forward declare the saver widget

// Define the variant type for saver options
using AnalogSaverOptionsVariant = std::variant<CSVAnalogSaverOptions>; // Will expand if more types are added

class AnalogTimeSeries_Widget : public QWidget {
    Q_OBJECT
public:
    explicit AnalogTimeSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~AnalogTimeSeries_Widget() override;

    void openWidget();// Call to open the widget
    void setActiveKey(std::string key);

private:
    Ui::AnalogTimeSeries_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;
    // int _callback_id{-1}; // If callbacks are needed in the future

    enum SaverType { CSV }; // Enum for different saver types

private slots:
    void _onExportTypeChanged(int index);
    void _handleSaveAnalogCSVRequested(CSVAnalogSaverOptions options);

private:
    void _initiateSaveProcess(SaverType saver_type, AnalogSaverOptionsVariant& options_variant);
    bool _performActualCSVSave(CSVAnalogSaverOptions & options);
};

#endif// ANALOGTIMESERIES_WIDGET_HPP
