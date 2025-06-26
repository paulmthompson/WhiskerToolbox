#ifndef NEWDATAWIDGET_HPP
#define NEWDATAWIDGET_HPP

#include <QString>
#include <QWidget>

#include <memory>
#include <string>


namespace Ui {
class NewDataWidget;
}

class DataManager;

class NewDataWidget : public QWidget {
    Q_OBJECT
public:
    explicit NewDataWidget(QWidget * parent = nullptr);
    ~NewDataWidget() override;

    /**
     * @brief Set the DataManager instance for this widget
     * 
     * @param data_manager Shared pointer to the DataManager instance
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Populate the timeframe combo box with available timeframes
     */
    void populateTimeframes();

    //void setDirLabel(QString const label);

signals:
    void createNewData(std::string key, std::string type, std::string timeframe_key);

private:
    Ui::NewDataWidget * ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _createNewData();
};


#endif// NEWDATAWIDGET_HPP
