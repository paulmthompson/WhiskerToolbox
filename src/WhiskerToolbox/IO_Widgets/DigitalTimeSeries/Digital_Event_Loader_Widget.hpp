#ifndef DIGITAL_EVENT_LOADER_WIDGET_HPP
#define DIGITAL_EVENT_LOADER_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QComboBox;
class QStackedWidget;
struct CSVEventLoaderOptions;
class DigitalEventSeries;

namespace Ui {
class Digital_Event_Loader_Widget;
}


class Digital_Event_Loader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Digital_Event_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Digital_Event_Loader_Widget() override;

private:
    Ui::Digital_Event_Loader_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    void _loadCSVEventData(std::vector<std::shared_ptr<DigitalEventSeries>> const & event_series_list,
                           CSVEventLoaderOptions const & options);

private slots:
    void _onLoaderTypeChanged(int index);
    void _handleLoadCSVEventRequested(CSVEventLoaderOptions options);
};

#endif// DIGITAL_EVENT_LOADER_WIDGET_HPP