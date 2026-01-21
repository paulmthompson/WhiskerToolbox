#include "MediaInterval_Widget.hpp"
#include "ui_MediaInterval_Widget.h"

#include "ColorPicker_Widget/ColorPicker_Widget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Media_Widget/Media_Window/Media_Window.hpp"
#include "Media_Widget/MediaWidgetState.hpp"
#include "StyleWidgets/BorderIntervalStyle_Widget.hpp"

#include <iostream>

MediaInterval_Widget::MediaInterval_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaInterval_Widget),
      _data_manager{std::move(data_manager)},
      _scene{std::move(scene)},
      _state{state} {
    ui->setupUi(this);

        connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaInterval_Widget::_setIntervalAlpha);
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaInterval_Widget::_setIntervalColor);
    
    // Connect plotting style selector
    connect(ui->plotting_style_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaInterval_Widget::_setPlottingStyle);
    
    // Initialize and setup sub-widgets
    ui->box_style_widget->setScene(_scene);
    ui->border_style_widget->setScene(_scene);
}

MediaInterval_Widget::~MediaInterval_Widget() {
    delete ui;
}

void MediaInterval_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

        // Set the color picker and controls to the current interval configuration if available
    if (!key.empty() && _state) {
        auto const * config = _state->displayOptions().get<DigitalIntervalDisplayOptions>(QString::fromStdString(key));

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config->hex_color()));
            ui->color_picker->setAlpha(static_cast<int>(config->alpha() * 100));
            
            // Set the plotting style and corresponding widget
            ui->plotting_style_combobox->setCurrentIndex(static_cast<int>(config->plotting_style));
            
            // Update the appropriate style widget based on current style
            switch (config->plotting_style) {
                case IntervalPlottingStyle::Box:
                    ui->box_style_widget->setActiveKey(key);
                    ui->box_style_widget->updateFromConfig(config);
                    break;
                case IntervalPlottingStyle::Border:
                    ui->border_style_widget->setActiveKey(key);
                    ui->border_style_widget->updateFromConfig(config);
                    break;
            }
        }
    }
}

void MediaInterval_Widget::_setIntervalAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * interval_opts = _state->displayOptions().getMutable<DigitalIntervalDisplayOptions>(key);
        if (interval_opts) {
            interval_opts->alpha() = alpha_float;
            _state->displayOptions().notifyChanged<DigitalIntervalDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaInterval_Widget::_setIntervalColor(QString const & hex_color) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * interval_opts = _state->displayOptions().getMutable<DigitalIntervalDisplayOptions>(key);
        if (interval_opts) {
            interval_opts->hex_color() = hex_color.toStdString();
            _state->displayOptions().notifyChanged<DigitalIntervalDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaInterval_Widget::_setPlottingStyle(int style_index) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * interval_opts = _state->displayOptions().getMutable<DigitalIntervalDisplayOptions>(key);
        if (interval_opts) {
            interval_opts->plotting_style = static_cast<IntervalPlottingStyle>(style_index);
            _state->displayOptions().notifyChanged<DigitalIntervalDisplayOptions>(key);
            
            // Switch to the appropriate style options widget
            ui->style_options_stack->setCurrentIndex(style_index);
            
            // Update the current style widget with the active key
            switch (static_cast<IntervalPlottingStyle>(style_index)) {
                case IntervalPlottingStyle::Box:
                    ui->box_style_widget->setActiveKey(_active_key);
                    ui->box_style_widget->updateFromConfig(interval_opts);
                    break;
                case IntervalPlottingStyle::Border:
                    ui->border_style_widget->setActiveKey(_active_key);
                    ui->border_style_widget->updateFromConfig(interval_opts);
                    break;
                // Future styles can be added here
            }
        }
        _scene->UpdateCanvas();
    }
}
