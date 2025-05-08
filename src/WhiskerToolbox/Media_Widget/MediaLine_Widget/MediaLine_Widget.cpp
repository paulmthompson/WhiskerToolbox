
#include "MediaLine_Widget.hpp"
#include "ui_MediaLine_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "Media_Window/Media_Window.hpp"

#include <iostream>

MediaLine_Widget::MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MediaLine_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene} {
    ui->setupUi(this);

    _selection_modes["(None)"] = std::make_pair(Selection_Mode::None, "Description:");
    _selection_modes["Add Points"] = std::make_pair(Selection_Mode::Add, "Description: Add points to end of selected line");

    ui->selection_mode_combo->addItems(QStringList(_selection_modes.keys()));
    
    connect(ui->selection_mode_combo, &QComboBox::currentTextChanged, this, &MediaLine_Widget::_toggleSelectionMode);

}

MediaLine_Widget::~MediaLine_Widget() {
    delete ui;
}

void MediaLine_Widget::showEvent(QShowEvent * event) {
    std::cout << "Show Event" << std::endl;
   connect(_scene, &Media_Window::leftClickMedia, this, &MediaLine_Widget::_clickedInVideo);

}

void MediaLine_Widget::hideEvent(QHideEvent * event) {
    std::cout << "Hide Event" << std::endl;
    disconnect(_scene, &Media_Window::leftClickMedia, this, &MediaLine_Widget::_clickedInVideo);
}


void MediaLine_Widget::setActiveKey(std::string const& key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));
}

void MediaLine_Widget::_clickedInVideo(qreal x_canvas, qreal y_canvas) {

    if (_active_key.empty()) {
        std::cout << "No active key" << std::endl;
        return;
    }

    if (!_data_manager->getData<LineData>(_active_key)) {
        std::cout << "No line data for active key" << std::endl;
        return;
    }

    float const x_media = static_cast<float>(x_canvas) / _scene->getXAspect();
    float const y_media = static_cast<float>(y_canvas) / _scene->getYAspect();
    auto const current_time = _data_manager->getTime()->getLastLoadedFrame();

    auto lines = _data_manager->getData<LineData>(_active_key)->getLinesAtTime(current_time);

    switch (_selection_mode) {
        case Selection_Mode::None: {
            std::cout << "Selection mode is None" << std::endl;
            break;
        }
        case Selection_Mode::Add: {
            std::cout << "Selection mode is Add" << std::endl;
            if (lines.empty()) {
                _data_manager->getData<LineData>(_active_key)->addLineAtTime(current_time, {{x_media, y_media}});
            } else {
                _data_manager->getData<LineData>(_active_key)->addPointToLineInterpolate(current_time, 0, Point2D<float>{x_media, y_media});
            }

            _scene->UpdateCanvas();

            std::cout << "Added point (" << x_media << ", " << y_media << ") to line " << _active_key << std::endl;
            break;
        }
    }

}

void MediaLine_Widget::_toggleSelectionMode(QString text) {

    auto selected_pair = _selection_modes[text];

    ui->selection_mode_description->setText(selected_pair.second);

    _selection_mode = selected_pair.first;
}

/*
void MediaLine_Widget::_clearCurrentLine() {
    if (_active_key.empty()) {
        return;
    }

    auto current_time = _data_manager->getTime()->getLastLoadedFrame();

    if (_data_manager->getData<LineData>(_active_key)) {
        _data_manager->getData<LineData>(_active_key)->clearLinesAtTime(current_time);
        _scene->UpdateCanvas();
        std::cout << "Cleared line " << _active_key << " at time " << current_time << std::endl;
    }
}
*/
