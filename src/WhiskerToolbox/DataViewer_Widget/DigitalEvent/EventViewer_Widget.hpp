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

signals:
    void colorChanged(std::string const & feature_key, std::string const & hex_color);
    void alphaChanged(std::string const & feature_key, float alpha);

private slots:
    void _setEventColor(const QString& hex_color);
    void _setEventAlpha(int alpha);
    
    /**
     * @brief Set the display mode for the active event series
     * @param mode_index Index from combo box (0=Stacked, 1=FullCanvas)
     */
    void _setDisplayMode(int mode_index);
    
    /**
     * @brief Set the vertical spacing between stacked event series
     * @param spacing Spacing value in normalized coordinates
     */
    void _setVerticalSpacing(double spacing);
    
    /**
     * @brief Set the height of individual event lines in stacked mode
     * @param height Height value in normalized coordinates
     */
    void _setEventHeight(double height);

private:
    Ui::EventViewer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    OpenGLWidget * _opengl_widget;
    std::string _active_key;
};

#endif// EVENTVIEWER_WIDGET_HPP 