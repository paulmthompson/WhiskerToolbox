#ifndef DIGITALEVENTSERIES_WIDGET_HPP
#define DIGITALEVENTSERIES_WIDGET_HPP

#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <variant>

namespace Ui {
class DigitalEventSeries_Widget;
}

class DataManager;
class CSVEventSaver_Widget;

using EventSaverOptionsVariant = std::variant<CSVEventSaverOptions>;

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

    enum SaverType { CSV };

    void _calculateEvents();
    void _assignCallbacks();

    void _initiateSaveProcess(SaverType saver_type, EventSaverOptionsVariant & options_variant);
    bool _performActualCSVSave(CSVEventSaverOptions const & options);

    /**
     * @brief Generate appropriate filename based on active key and export type
     * 
     * @return String containing the filename with appropriate extension
     */
    std::string _generateFilename() const;

    /**
     * @brief Update the filename field based on current active key and export type
     */
    void _updateFilename();

private slots:
    void _addEventButton();
    void _removeEventButton();

    void _onExportTypeChanged(int index);
    void _handleSaveEventCSVRequested(CSVEventSaverOptions options);
};


#endif// DIGITALEVENTSERIES_WIDGET_HPP
