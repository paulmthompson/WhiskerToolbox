#ifndef MEDIALINE_WIDGET_HPP
#define MEDIALINE_WIDGET_HPP


#include <QWidget>
#include <memory>
#include <string>

namespace Ui {
class MediaLine_Widget;
}

class DataManager;
class Media_Window;

class MediaLine_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent = nullptr);
    ~MediaLine_Widget() override;

    void setActiveKey(std::string const& key);

private:
    Ui::MediaLine_Widget* ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window* _scene;
    std::string _active_key;
};

#endif// MEDIALINE_WIDGET_HPP
