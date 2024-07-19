#ifndef MEDIA_WIDGET_ITEMS_HPP
#define MEDIA_WIDGET_ITEMS_HPP

#include <QWidget>

class DataManager;
class Media_Window;

namespace Ui {
class Media_Widget_Items;
}

class Media_Widget_Items : public QWidget
{
    Q_OBJECT
public:

    explicit Media_Widget_Items(QWidget *parent = 0);

    virtual ~Media_Widget_Items();

    void setDataManager(std::shared_ptr<DataManager> data_manager) {_data_manager = data_manager;};
    void setScene(Media_Window* scene) {_scene = scene;};

protected:
private:
    Ui::Media_Widget_Items *ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window* _scene;

};

#endif // MEDIA_WIDGET_ITEMS_HPP
