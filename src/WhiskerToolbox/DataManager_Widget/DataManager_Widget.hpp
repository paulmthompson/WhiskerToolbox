#ifndef DATAMANAGER_WIDGET_HPP
#define DATAMANAGER_WIDGET_HPP

#include <QString>
#include <QWidget>

#include <memory>
#include <string>


namespace Ui {
class DataManager_Widget;
}

class DataManager;
class TimeScrollBar;

class DataManager_Widget : public QWidget {
    Q_OBJECT
public:
    DataManager_Widget(std::shared_ptr<DataManager> data_manager,
                       TimeScrollBar * time_scrollbar,
                       QWidget * parent = nullptr);
    ~DataManager_Widget() override;

    void openWidget();// Call

private:
    Ui::DataManager_Widget * ui;
    TimeScrollBar * _time_scrollbar;
    std::shared_ptr<DataManager> _data_manager;
    QString _highlighted_available_feature;
    std::vector<int> _current_data_callbacks;


private slots:
    void _changeOutputDir(QString dir_name);
    void _handleFeatureSelected(QString const & feature);
    void _disablePreviousFeature(QString const & feature);
    void _createNewData(std::string key, std::string type);
    void _changeScrollbar(int frame_id);
};


#endif// DATAMANAGER_WIDGET_HPP
