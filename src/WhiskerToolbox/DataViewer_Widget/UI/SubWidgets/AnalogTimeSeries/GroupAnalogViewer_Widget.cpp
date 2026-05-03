/**
 * @file GroupAnalogViewer_Widget.cpp
 * @brief Batch property editor for a group of analog time series in DataViewer.
 */

#include "GroupAnalogViewer_Widget.hpp"

#include "DataViewer_Widget/Core/DataViewerState.hpp"
#include "DataViewer_Widget/Core/DataViewerStateData.hpp"
#include "DataViewer_Widget/Rendering/OpenGLWidget.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

GroupAnalogViewer_Widget::GroupAnalogViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                                   OpenGLWidget * opengl_widget,
                                                   QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget} {
    auto * main_layout = new QVBoxLayout(this);

    // Type row
    auto * type_row = new QHBoxLayout();
    type_row->addWidget(new QLabel("Type:", this));
    auto * type_value = new QLabel("Analog Group", this);
    type_value->setAlignment(Qt::AlignCenter);
    type_row->addWidget(type_value);
    main_layout->addLayout(type_row);

    // Name row
    auto * name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel("Group:", this));
    _name_label = new QLabel("—", this);
    _name_label->setAlignment(Qt::AlignCenter);
    name_row->addWidget(_name_label);
    main_layout->addLayout(name_row);

    // Count row
    auto * count_row = new QHBoxLayout();
    count_row->addWidget(new QLabel("Channels:", this));
    _count_label = new QLabel("0", this);
    _count_label->setAlignment(Qt::AlignCenter);
    count_row->addWidget(_count_label);
    main_layout->addLayout(count_row);

    // Color row
    auto * color_row = new QHBoxLayout();
    color_row->addWidget(new QLabel("Color:", this));
    _color_display_button = new QPushButton(this);
    _color_display_button->setFixedSize(30, 30);
    _color_display_button->setFlat(false);
    _color_display_button->setEnabled(false);
    color_row->addWidget(_color_display_button);
    _color_button = new QPushButton("Set All...", this);
    color_row->addWidget(_color_button);
    color_row->addStretch();
    main_layout->addLayout(color_row);

    // Alpha row
    auto * alpha_row = new QHBoxLayout();
    alpha_row->addWidget(new QLabel("Alpha:", this));
    _alpha_slider = new QSlider(Qt::Horizontal, this);
    _alpha_slider->setRange(0, 100);
    _alpha_slider->setValue(100);
    alpha_row->addWidget(_alpha_slider);
    _alpha_value_label = new QLabel("1.00", this);
    _alpha_value_label->setMinimumWidth(30);
    _alpha_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    alpha_row->addWidget(_alpha_value_label);
    main_layout->addLayout(alpha_row);

    // Scale row
    auto * scale_row = new QHBoxLayout();
    scale_row->addWidget(new QLabel("Y Scale:", this));
    _scale_spinbox = new QDoubleSpinBox(this);
    _scale_spinbox->setRange(0.1, 10.0);
    _scale_spinbox->setSingleStep(0.1);
    _scale_spinbox->setValue(1.0);
    scale_row->addWidget(_scale_spinbox);
    main_layout->addLayout(scale_row);

    // Thickness row
    auto * thickness_row = new QHBoxLayout();
    thickness_row->addWidget(new QLabel("Thickness:", this));
    _thickness_spinbox = new QDoubleSpinBox(this);
    _thickness_spinbox->setRange(0.1, 10.0);
    _thickness_spinbox->setSingleStep(0.1);
    _thickness_spinbox->setDecimals(1);
    _thickness_spinbox->setSuffix(" px");
    _thickness_spinbox->setValue(1.0);
    thickness_row->addWidget(_thickness_spinbox);
    main_layout->addLayout(thickness_row);

    _min_max_decimation_checkbox = new QCheckBox(
            QStringLiteral("Min–max decimation (dense traces)"), this);
    _min_max_decimation_checkbox->setToolTip(
            QStringLiteral("Reduce plotted points per channel to match horizontal resolution. "
                           "Applies to every series in this group."));
    main_layout->addWidget(_min_max_decimation_checkbox);

    main_layout->addStretch();

    // Connections
    connect(_color_button, &QPushButton::clicked, this, &GroupAnalogViewer_Widget::_openColorDialog);
    connect(_alpha_slider, &QSlider::valueChanged, this, &GroupAnalogViewer_Widget::_onAlphaChanged);
    connect(_scale_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GroupAnalogViewer_Widget::_onScaleChanged);
    connect(_thickness_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GroupAnalogViewer_Widget::_onThicknessChanged);
    connect(_min_max_decimation_checkbox, &QCheckBox::toggled, this,
            &GroupAnalogViewer_Widget::_onMinMaxDecimationToggled);
}

GroupAnalogViewer_Widget::~GroupAnalogViewer_Widget() = default;

void GroupAnalogViewer_Widget::setActiveKeys(std::string const & group_name,
                                             std::vector<std::string> const & keys) {
    _group_name = group_name;
    _active_keys = keys;
    _name_label->setText(QString::fromStdString(group_name));
    _count_label->setText(QString::number(static_cast<int>(keys.size())));

    if (keys.empty() || !_opengl_widget) {
        if (_min_max_decimation_checkbox != nullptr) {
            _min_max_decimation_checkbox->setChecked(false);
        }
        return;
    }

    // Read values from the first child key to populate defaults
    _updating = true;
    auto const * opts = _opengl_widget->state()->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(keys.front()));
    if (opts) {
        _color_display_button->setStyleSheet(
                QString("QPushButton { background-color: %1; border: 1px solid #808080; }")
                        .arg(QString::fromStdString(opts->hex_color())));
        _alpha_slider->setValue(static_cast<int>(opts->get_alpha() * 100.0f));
        _alpha_value_label->setText(QString::number(static_cast<double>(opts->get_alpha()), 'f', 2));
        _scale_spinbox->setValue(static_cast<double>(opts->user_scale_factor));
        _thickness_spinbox->setValue(static_cast<double>(opts->get_line_thickness()));
        _min_max_decimation_checkbox->setChecked(opts->enable_min_max_line_decimation);
    } else {
        _min_max_decimation_checkbox->setChecked(false);
    }
    _updating = false;
}

void GroupAnalogViewer_Widget::_openColorDialog() {
    if (_active_keys.empty()) {
        return;
    }

    QColor current_color("#0000FF");
    auto const * opts = _opengl_widget->state()->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(_active_keys.front()));
    if (opts) {
        current_color = QColor(QString::fromStdString(opts->hex_color()));
    }

    QColor const color = QColorDialog::getColor(current_color, this, "Choose Color for Group");
    if (!color.isValid()) {
        return;
    }

    std::string const hex = color.name().toStdString();
    _color_display_button->setStyleSheet(
            QString("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(color.name()));

    _applyToAllKeys([this, &hex](std::string const & key) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            opts->hex_color() = hex;
            emit colorChanged(key, hex);
        }
    });
    _opengl_widget->update();
}

void GroupAnalogViewer_Widget::_onAlphaChanged(int value) {
    if (_updating || _active_keys.empty()) {
        return;
    }
    float const alpha = static_cast<float>(value) / 100.0f;
    _alpha_value_label->setText(QString::number(static_cast<double>(alpha), 'f', 2));

    _applyToAllKeys([this, alpha](std::string const & key) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            opts->alpha() = alpha;
        }
    });
    _opengl_widget->update();
}

void GroupAnalogViewer_Widget::_onScaleChanged(double value) {
    if (_updating || _active_keys.empty()) {
        return;
    }

    _applyToAllKeys([this, value](std::string const & key) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            opts->user_scale_factor = static_cast<float>(value);
        }
    });
    _opengl_widget->update();
}

void GroupAnalogViewer_Widget::_onThicknessChanged(double value) {
    if (_updating || _active_keys.empty()) {
        return;
    }

    _applyToAllKeys([this, value](std::string const & key) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            opts->line_thickness() = static_cast<float>(value);
        }
    });
    _opengl_widget->update();
}

/// @brief Enable or disable min–max decimation for every channel in the active group.
void GroupAnalogViewer_Widget::_onMinMaxDecimationToggled(bool enabled) {
    if (_updating || _active_keys.empty()) {
        return;
    }

    _applyToAllKeys([this, enabled](std::string const & key) {
        auto * opts = _opengl_widget->state()->seriesOptions().getMutable<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            opts->enable_min_max_line_decimation = enabled;
        }
    });
    _opengl_widget->update();
}

void GroupAnalogViewer_Widget::_applyToAllKeys(std::function<void(std::string const &)> const & fn) {
    for (auto const & key: _active_keys) {
        fn(key);
    }
}
