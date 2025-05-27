#ifndef ANALOGVIEWER_WIDGET_HPP
#define ANALOGVIEWER_WIDGET_HPP

#include <QWidget>

#include <string>

namespace Ui {
class AnalogViewer_Widget;
}

class DataManager;
class OpenGLWidget;

class AnalogViewer_Widget : public QWidget {
    Q_OBJECT
public:
    explicit AnalogViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent = nullptr);
    ~AnalogViewer_Widget() override;

    void setActiveKey(std::string const & key);

private:
    Ui::AnalogViewer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget;
    std::string _active_key;
};

#endif// ANALOGVIEWER_WIDGET_HPP 