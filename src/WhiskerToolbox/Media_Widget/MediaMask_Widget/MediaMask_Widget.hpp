#ifndef MEDIAMASK_WIDGET_HPP
#define MEDIAMASK_WIDGET_HPP

#include <QWidget>

#include <string>

namespace Ui {
class MediaMask_Widget;
}

class DataManager;

class MediaMask_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaMask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~MediaMask_Widget() override;

    void openWidget();// Call
    void setActiveKey(std::string const & key);

private:
    Ui::MediaMask_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;

private slots:
};


#endif// MEDIAMASK_WIDGET_HPP
