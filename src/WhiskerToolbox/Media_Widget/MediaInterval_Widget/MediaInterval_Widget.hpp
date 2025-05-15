#ifndef MEDIAINTERVAL_WIDGET_HPP
#define MEDIAINTERVAL_WIDGET_HPP

#include <QWidget>

#include <string>

namespace Ui {
class MediaInterval_Widget;
}

class DataManager;
class Media_Window;

class MediaInterval_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaInterval_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaInterval_Widget() override;

    void setActiveKey(std::string const & key);

private:
    Ui::MediaInterval_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;

private slots:
    void _setIntervalAlpha(int alpha);
    void _setIntervalColor(const QString& hex_color);
};

#endif// MEDIAINTERVAL_WIDGET_HPP
