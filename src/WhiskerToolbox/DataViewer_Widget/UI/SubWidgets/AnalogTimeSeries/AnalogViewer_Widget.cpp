#include "AnalogViewer_Widget.hpp"
#include "ui_AnalogViewer_Widget.h"

#include "DataViewer_Widget/Core/DataViewerState.hpp"
#include "DataViewer_Widget/Core/DataViewerStateData.hpp"
#include "DataViewer_Widget/Rendering/OpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <cctype>
#include <iostream>

AnalogViewer_Widget::AnalogViewer_Widget(std::shared_ptr<DataManager> data_manager, OpenGLWidget * opengl_widget, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::AnalogViewer_Widget),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget} {
    ui->setupUi(this);

    // Set the color display button to be flat and show just the color
    ui->color_display_button->setFlat(false);
    ui->color_display_button->setEnabled(false);// Make it non-clickable, just for display

    connect(ui->color_button, &QPushButton::clicked,
            this, &AnalogViewer_Widget::_openColorDialog);
    connect(ui->alpha_slider, &QSlider::valueChanged,
            this, &AnalogViewer_Widget::_setAnalogAlpha);
    connect(ui->scale_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogViewer_Widget::_setAnalogScaleFactor);
    connect(ui->line_thickness_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogViewer_Widget::_setLineThickness);

    // Connect gap handling controls
    connect(ui->gap_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalogViewer_Widget::_setGapHandlingMode);
    connect(ui->gap_threshold_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AnalogViewer_Widget::_setGapThreshold);

    // Create std dev display labels (inserted before vertical spacer)
    auto * main_layout = qobject_cast<QVBoxLayout *>(layout());
    if (main_layout) {
        // Find the spacer index to insert before it
        int const spacer_index = main_layout->count() - 1;

        auto * std_dev_row = new QHBoxLayout();
        auto * std_dev_title = new QLabel("Std Dev:", this);
        _std_dev_label = new QLabel("—", this);
        _std_dev_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        std_dev_row->addWidget(std_dev_title);
        std_dev_row->addWidget(_std_dev_label);
        main_layout->insertLayout(spacer_index, std_dev_row);

        auto * group_row = new QHBoxLayout();
        auto * group_title = new QLabel("Group Std Dev:", this);
        _group_std_dev_label = new QLabel("—", this);
        _group_std_dev_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        group_row->addWidget(group_title);
        group_row->addWidget(_group_std_dev_label);
        main_layout->insertLayout(spacer_index + 1, group_row);
    }
}

AnalogViewer_Widget::~AnalogViewer_Widget() {
    delete ui;
}

void AnalogViewer_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color and scale factor to current values from state if available
    if (!key.empty()) {
        auto const * opts = _opengl_widget->state()->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            _updateColorDisplay(QString::fromStdString(opts->hex_color()));

            // Set alpha from state
            auto const alpha_int = static_cast<int>(opts->get_alpha() * 100.0f);
            ui->alpha_slider->setValue(alpha_int);
            ui->alpha_value_label->setText(QString::number(static_cast<double>(opts->get_alpha()), 'f', 2));

            // Set scale factor from state
            ui->scale_spinbox->setValue(static_cast<double>(opts->user_scale_factor));

            // Set line thickness
            ui->line_thickness_spinbox->setValue(static_cast<double>(opts->get_line_thickness()));

            // Set gap handling controls
            ui->gap_mode_combo->setCurrentIndex(static_cast<int>(opts->gap_handling));
            ui->gap_threshold_spinbox->setValue(static_cast<int>(opts->gap_threshold));
        } else {
            _updateColorDisplay("#0000FF");           // Default blue
            ui->scale_spinbox->setValue(1.0);         // Default scale
            ui->line_thickness_spinbox->setValue(1.0);// Default line thickness
            ui->gap_mode_combo->setCurrentIndex(0);   // Default to AlwaysConnect
            ui->gap_threshold_spinbox->setValue(5);   // Default threshold (5 frames)
        }
    }

    _updateStdDevDisplay();

    std::cout << "AnalogViewer_Widget: Active key set to " << key << std::endl;
}

void AnalogViewer_Widget::_openColorDialog() {
    if (_active_key.empty()) {
        return;
    }

    // Get current color
    QColor currentColor;
    auto const * opts = _opengl_widget->state()->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(_active_key));
    if (opts) {
        currentColor = QColor(QString::fromStdString(opts->hex_color()));
    } else {
        currentColor = QColor("#0000FF");
    }

    // Open color dialog
    QColor const color = QColorDialog::getColor(currentColor, this, "Choose Color");

    if (color.isValid()) {
        QString const hex_color = color.name();
        _updateColorDisplay(hex_color);
        _setAnalogColor(hex_color);
    }
}

void AnalogViewer_Widget::_updateColorDisplay(QString const & hex_color) {
    // Update the color display button with the new color
    ui->color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(hex_color));
}

void AnalogViewer_Widget::_setAnalogColor(QString const & hex_color) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->hex_color() = hex_color.toStdString();
            emit colorChanged(_active_key, hex_color.toStdString());
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setAnalogAlpha(int alpha) {
    if (!_active_key.empty()) {
        float const alpha_float = static_cast<float>(alpha) / 100.0f;
        ui->alpha_value_label->setText(QString::number(static_cast<double>(alpha_float), 'f', 2));
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->alpha() = alpha_float;
            emit alphaChanged(_active_key, alpha_float);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setAnalogScaleFactor(double scale_factor) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->user_scale_factor = static_cast<float>(scale_factor);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setLineThickness(double thickness) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->line_thickness() = static_cast<float>(thickness);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setGapHandlingMode(int mode_index) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->gap_handling = static_cast<AnalogGapHandlingMode>(mode_index);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_setGapThreshold(int threshold) {
    if (!_active_key.empty()) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(_active_key));
        if (opts) {
            opts->gap_threshold = static_cast<float>(threshold);
            // Trigger immediate repaint
            _opengl_widget->update();
        }
    }
}

void AnalogViewer_Widget::_updateStdDevDisplay() {
    if (!_std_dev_label || !_group_std_dev_label) {
        return;
    }

    if (_active_key.empty() || !_opengl_widget) {
        _std_dev_label->setText(QString::fromUtf8("—"));
        _group_std_dev_label->setText(QString::fromUtf8("—"));
        return;
    }

    // Get data cache from the analog series map
    auto const & analog_map = _opengl_widget->getAnalogSeriesMap();
    auto it = analog_map.find(_active_key);
    if (it == analog_map.end()) {
        _std_dev_label->setText(QString::fromUtf8("—"));
        _group_std_dev_label->setText(QString::fromUtf8("—"));
        return;
    }

    auto const & cache = it->second.data_cache;

    // Show individual std_dev (the original per-series value)
    _std_dev_label->setText(QString::number(static_cast<double>(cache.individual_std_dev), 'g', 4));

    // Show group std_dev if available
    // Extract group name from active key
    std::string group_name;
    auto const pos = _active_key.rfind('_');
    if (pos != std::string::npos && pos > 0 && pos + 1 < _active_key.size()) {
        bool all_digits = true;
        for (size_t i = pos + 1; i < _active_key.size(); ++i) {
            if (std::isdigit(static_cast<unsigned char>(_active_key[i])) == 0) {
                all_digits = false;
                break;
            }
        }
        if (all_digits) {
            group_name = _active_key.substr(0, pos);
        }
    }

    if (!group_name.empty()) {
        auto const * gs = _opengl_widget->state()->getGroupScaling(group_name);
        if (gs && gs->group_std_dev > 0.0f) {
            QString label = QString::number(static_cast<double>(gs->group_std_dev), 'g', 4);
            if (gs->unified_scaling) {
                label += " (active)";
            } else {
                label += " (disabled)";
            }
            _group_std_dev_label->setText(label);
        } else {
            _group_std_dev_label->setText(QString::fromUtf8("—"));
        }
    } else {
        _group_std_dev_label->setText(QString::fromUtf8("N/A"));
    }
}
