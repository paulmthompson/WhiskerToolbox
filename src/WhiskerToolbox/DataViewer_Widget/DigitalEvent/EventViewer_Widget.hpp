#ifndef EVENTVIEWER_WIDGET_HPP
#define EVENTVIEWER_WIDGET_HPP

#include <QWidget>

#include <string>

namespace Ui {
class EventViewer_Widget;
}

class DataManager;
class OpenGLWidget;

class EventViewer_Widget : public QWidget {
    Q_OBJECT
public:
    explicit EventViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent = nullptr);
    ~EventViewer_Widget() override;

    void setActiveKey(std::string const & key);

private:
    Ui::EventViewer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget;
    std::string _active_key;
};

#endif// EVENTVIEWER_WIDGET_HPP 