#ifndef MEDIA_WIDGET_HPP
#define MEDIA_WIDGET_HPP

#include <QWidget>

class DataManager;
class Media_Window;

namespace Ui {
class Media_Widget;
}

class Media_Widget : public QWidget
{
    Q_OBJECT
public:

    explicit Media_Widget(QWidget *parent = 0);

    virtual ~Media_Widget();

    void setDataManager(std::shared_ptr<DataManager> data_manager) {_data_manager = data_manager;};
    void setScene(Media_Window* scene) {_scene = scene;};

    void updateMedia();

protected:
private:
    Ui::Media_Widget *ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window* _scene;




private slots:

signals:

};

#endif // MEDIA_WIDGET_HPP
