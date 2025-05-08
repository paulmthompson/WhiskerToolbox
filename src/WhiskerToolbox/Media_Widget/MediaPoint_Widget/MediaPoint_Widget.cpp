#include "MediaPoint_Widget.hpp"
#include "ui_MediaPoint_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Media_Window/Media_Window.hpp"


#include <iostream>

MediaPoint_Widget::MediaPoint_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaPoint_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene}
{
    ui->setupUi(this);

}

MediaPoint_Widget::~MediaPoint_Widget() {
    delete ui;
}

void MediaPoint_Widget::showEvent(QShowEvent * event) {
    std::cout << "Show Event" << std::endl;
    connect(_scene, &Media_Window::leftClickMedia, this, &MediaPoint_Widget::_assignPoint);

}

void MediaPoint_Widget::hideEvent(QHideEvent * event) {
    std::cout << "Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickMedia, this, &MediaPoint_Widget::_assignPoint);
}

void MediaPoint_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
    _selection_enabled = !key.empty();

}


void MediaPoint_Widget::_assignPoint(qreal x_media, qreal y_media) {
    if (!_selection_enabled || _active_key.empty())
        return;

    auto frame_id = _data_manager->getTime()->getLastLoadedFrame();

    auto point = _data_manager->getData<PointData>(_active_key);
    if (point) {

        point->overwritePointAtTime(frame_id, {.x = static_cast<float>(y_media),
                                               .y = static_cast<float>(x_media)
                                              });

        _scene->UpdateCanvas();
    }
}
