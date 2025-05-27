#ifndef INTERVALVIEWER_WIDGET_HPP
#define INTERVALVIEWER_WIDGET_HPP

#include <QWidget>

#include <string>

namespace Ui {
class IntervalViewer_Widget;
}

class DataManager;
class OpenGLWidget;

class IntervalViewer_Widget : public QWidget {
    Q_OBJECT
public:
    explicit IntervalViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent = nullptr);
    ~IntervalViewer_Widget() override;

    void setActiveKey(std::string const & key);

private:
    Ui::IntervalViewer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget;
    std::string _active_key;
};

#endif// INTERVALVIEWER_WIDGET_HPP 