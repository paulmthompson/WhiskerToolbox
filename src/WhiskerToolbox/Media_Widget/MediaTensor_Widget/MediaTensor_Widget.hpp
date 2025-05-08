#ifndef MEDIATENSOR_WIDGET_HPP
#define MEDIATENSOR_WIDGET_HPP


#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class MediaTensor_Widget;
}

class DataManager;
class Media_Window;

class MediaTensor_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaTensor_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaTensor_Widget() override;

    void setActiveKey(std::string const & key);

private:
    Ui::MediaTensor_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;

private slots:
    void _setTensorChannel(int channel);
};


#endif// MEDIATENSOR_WIDGET_HPP
