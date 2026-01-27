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

signals:
    void colorChanged(std::string const & feature_key, std::string const & hex_color);
    void alphaChanged(std::string const & feature_key, float alpha);

private slots:
    void _openColorDialog();
    void _setAnalogColor(QString const & hex_color);
    void _setAnalogAlpha(int alpha);
    void _setAnalogScaleFactor(double scale_factor);
    void _setLineThickness(int thickness);
    void _setGapHandlingMode(int mode_index);
    void _setGapThreshold(int threshold);

private:
    void _updateColorDisplay(QString const & hex_color);

    Ui::AnalogViewer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget;
    std::string _active_key;
};

#endif// ANALOGVIEWER_WIDGET_HPP