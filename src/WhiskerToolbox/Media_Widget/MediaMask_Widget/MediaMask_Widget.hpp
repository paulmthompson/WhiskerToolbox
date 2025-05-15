#ifndef MEDIAMASK_WIDGET_HPP
#define MEDIAMASK_WIDGET_HPP

#include <QWidget>

#include <string>

namespace Ui {
class MediaMask_Widget;
}

class DataManager;
class Media_Window;

class MediaMask_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaMask_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaMask_Widget() override;

    void setActiveKey(std::string const & key);

private:
    Ui::MediaMask_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;

private slots:
    void _setMaskAlpha(int alpha);
    void _setMaskColor(const QString& hex_color);
};


#endif// MEDIAMASK_WIDGET_HPP
