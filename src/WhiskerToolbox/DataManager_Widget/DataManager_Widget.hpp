#ifndef DATAMANAGER_WIDGET_HPP
#define DATAMANAGER_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>


namespace Ui { class DataManager_Widget; }

class DataManager;

class DataManager_Widget : public QWidget
{
    Q_OBJECT
public:

    DataManager_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent = 0);
    ~DataManager_Widget();

private:
    Ui::DataManager_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:

};


#endif // DATAMANAGER_WIDGET_HPP
