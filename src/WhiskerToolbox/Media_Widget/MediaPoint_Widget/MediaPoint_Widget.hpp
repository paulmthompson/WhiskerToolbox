#ifndef MEDIAPOINT_WIDGET_HPP
#define MEDIAPOINT_WIDGET_HPP

#include <QWidget>

#include <string>

namespace Ui {
class MediaPoint_Widget;
}

class DataManager;
class Media_Window;

class MediaPoint_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaPoint_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaPoint_Widget() override;

    void setActiveKey(std::string const & key);

private:
    Ui::MediaPoint_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;

private slots:
};

#endif// MEDIAPOINT_WIDGET_HPP
