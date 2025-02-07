#ifndef DATAMANAGER_WIDGET_HPP
#define DATAMANAGER_WIDGET_HPP

#include <QWidget>


#include <memory>
#include <string>


namespace Ui { class DataManager_Widget; }

class DataManager;
class Media_Window;
class TimeScrollBar;

class DataManager_Widget : public QWidget
{
    Q_OBJECT
public:

    DataManager_Widget(Media_Window* scene,
                       std::shared_ptr<DataManager> data_manager,
                       TimeScrollBar* time_scrollbar,
                       QWidget *parent = 0);
    ~DataManager_Widget();

    void openWidget(); // Call

private:
    Ui::DataManager_Widget *ui;
    Media_Window* _scene;
    TimeScrollBar* _time_scrollbar;
    std::shared_ptr<DataManager> _data_manager;
    QString _highlighted_available_feature;
    std::vector<int> _current_data_callbacks;


private slots:
    void _changeOutputDir();
    void _handleFeatureSelected(const QString& feature);
    void _disablePreviousFeature(const QString& feature);
    void _createNewData();
    void _changeScrollbar(int frame_id);

};


#endif // DATAMANAGER_WIDGET_HPP
