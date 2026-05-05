/**
 * @file GroupIntervalViewer_Widget.cpp
 * @brief Batch property editor for a group of digital interval series in DataViewer.
 */

#include "GroupIntervalViewer_Widget.hpp"

#include "DataViewer_Widget/Core/DataViewerState.hpp"
#include "DataViewer_Widget/Core/DataViewerStateData.hpp"
#include "DataViewer_Widget/Rendering/OpenGLWidget.hpp"

#include "../GroupViewerUniqueRandomColors.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

GroupIntervalViewer_Widget::GroupIntervalViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                                       OpenGLWidget * opengl_widget,
                                                       QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      _opengl_widget{opengl_widget} {
    auto * main_layout = new QVBoxLayout(this);

    auto * type_row = new QHBoxLayout();
    type_row->addWidget(new QLabel("Type:", this));
    auto * type_value = new QLabel("Interval Group", this);
    type_value->setAlignment(Qt::AlignCenter);
    type_row->addWidget(type_value);
    main_layout->addLayout(type_row);

    auto * name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel("Group:", this));
    _name_label = new QLabel(QStringLiteral("—"), this);
    _name_label->setAlignment(Qt::AlignCenter);
    name_row->addWidget(_name_label);
    main_layout->addLayout(name_row);

    auto * count_row = new QHBoxLayout();
    count_row->addWidget(new QLabel("Series:", this));
    _count_label = new QLabel(QStringLiteral("0"), this);
    _count_label->setAlignment(Qt::AlignCenter);
    count_row->addWidget(_count_label);
    main_layout->addLayout(count_row);

    auto * color_row = new QHBoxLayout();
    color_row->addWidget(new QLabel("Color:", this));
    _color_display_button = new QPushButton(this);
    _color_display_button->setFixedSize(30, 30);
    _color_display_button->setFlat(false);
    _color_display_button->setEnabled(false);
    color_row->addWidget(_color_display_button);
    _color_button = new QPushButton(QStringLiteral("Set All..."), this);
    color_row->addWidget(_color_button);
    color_row->addStretch();
    main_layout->addLayout(color_row);

    _random_unique_colors_button =
            new QPushButton(QStringLiteral("Unique random colors"), this);
    _random_unique_colors_button->setToolTip(
            QStringLiteral("Assign a different color to each series in this group (evenly spaced hues)"));
    main_layout->addWidget(_random_unique_colors_button);

    auto * alpha_row = new QHBoxLayout();
    alpha_row->addWidget(new QLabel("Alpha:", this));
    _alpha_slider = new QSlider(Qt::Horizontal, this);
    _alpha_slider->setRange(0, 100);
    _alpha_slider->setValue(100);
    alpha_row->addWidget(_alpha_slider);
    _alpha_value_label = new QLabel(QStringLiteral("1.00"), this);
    _alpha_value_label->setMinimumWidth(30);
    _alpha_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    alpha_row->addWidget(_alpha_value_label);
    main_layout->addLayout(alpha_row);

    auto * layout_row = new QHBoxLayout();
    layout_row->addWidget(new QLabel(QStringLiteral("Layout:"), this));
    _layout_mode_combo = new QComboBox(this);
    _layout_mode_combo->addItem(QStringLiteral("Full viewport (overlay)"));
    _layout_mode_combo->addItem(QStringLiteral("Stacked lanes"));
    _layout_mode_combo->setToolTip(
            QStringLiteral("Overlay spans the full plot height. Stacked assigns each series its own lane; "
                           "rectangle width follows interval duration."));
    layout_row->addWidget(_layout_mode_combo);
    main_layout->addLayout(layout_row);

    main_layout->addStretch();

    connect(_color_button, &QPushButton::clicked, this, &GroupIntervalViewer_Widget::_openColorDialog);
    connect(_random_unique_colors_button, &QPushButton::clicked, this,
            &GroupIntervalViewer_Widget::_assignRandomUniqueColors);
    connect(_alpha_slider, &QSlider::valueChanged, this, &GroupIntervalViewer_Widget::_onAlphaChanged);
    connect(_layout_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &GroupIntervalViewer_Widget::_onLayoutModeChanged);
}

GroupIntervalViewer_Widget::~GroupIntervalViewer_Widget() = default;

void GroupIntervalViewer_Widget::setActiveKeys(std::string const & group_name,
                                               std::vector<std::string> const & keys) {
    _group_name = group_name;
    _active_keys = keys;
    _name_label->setText(QString::fromStdString(group_name));
    _count_label->setText(QString::number(static_cast<int>(keys.size())));

    bool const can_edit = !keys.empty() && (_opengl_widget != nullptr);
    _color_button->setEnabled(can_edit);
    _random_unique_colors_button->setEnabled(can_edit);
    if (_layout_mode_combo != nullptr) {
        _layout_mode_combo->setEnabled(can_edit);
    }

    if (keys.empty() || !_opengl_widget) {
        return;
    }

    _updating = true;
    auto const * opts = _opengl_widget->state()->seriesOptions().get<DigitalIntervalSeriesOptionsData>(
            QString::fromStdString(keys.front()));
    if (opts) {
        _color_display_button->setStyleSheet(
                QStringLiteral("QPushButton { background-color: %1; border: 1px solid #808080; }")
                        .arg(QString::fromStdString(opts->hex_color())));
        _alpha_slider->setValue(static_cast<int>(opts->get_alpha() * 100.0f));
        _alpha_value_label->setText(QString::number(static_cast<double>(opts->get_alpha()), 'f', 2));
        if (_layout_mode_combo != nullptr) {
            QSignalBlocker const layout_block{_layout_mode_combo};
            _layout_mode_combo->setCurrentIndex(opts->extend_full_canvas ? 0 : 1);
        }
    }
    _updating = false;
}

/**
 * @brief Prompt for a color and assign it to every key in `_active_keys`
 */
void GroupIntervalViewer_Widget::_openColorDialog() {
    if (_active_keys.empty()) {
        return;
    }

    QColor current_color(QStringLiteral("#FF0000"));
    auto const * opts = _opengl_widget->state()->seriesOptions().get<DigitalIntervalSeriesOptionsData>(
            QString::fromStdString(_active_keys.front()));
    if (opts) {
        current_color = QColor(QString::fromStdString(opts->hex_color()));
    }

    QColor const color = QColorDialog::getColor(current_color, this, QStringLiteral("Choose Color for Group"));
    if (!color.isValid()) {
        return;
    }

    std::string const hex = color.name().toStdString();
    _color_display_button->setStyleSheet(
            QStringLiteral("QPushButton { background-color: %1; border: 1px solid #808080; }").arg(color.name()));

    _applyToAllKeys([this, &hex](std::string const & key) {
        auto * mutable_opts =
                _opengl_widget->state()->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(
                        QString::fromStdString(key));
        if (mutable_opts) {
            mutable_opts->hex_color() = hex;
            emit colorChanged(key, hex);
        }
    });
    _opengl_widget->update();
}

/**
 * @brief Pick distinct saturated colors on the hue wheel (random rotation) and apply per key
 */
void GroupIntervalViewer_Widget::_assignRandomUniqueColors() {
    if (_active_keys.empty() || !_opengl_widget) {
        return;
    }

    std::vector<std::string> const hex_colors = uniqueRandomHueWheelHexColors(_active_keys.size());
    for (std::size_t i = 0; i < _active_keys.size(); ++i) {
        std::string const & key = _active_keys[i];
        auto * mutable_opts =
                _opengl_widget->state()->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(
                        QString::fromStdString(key));
        if (mutable_opts) {
            std::string const & hex = hex_colors[i];
            mutable_opts->hex_color() = hex;
            emit colorChanged(key, hex);
        }
    }

    if (auto const * first_opts = _opengl_widget->state()->seriesOptions().get<DigitalIntervalSeriesOptionsData>(
                QString::fromStdString(_active_keys.front()))) {
        _color_display_button->setStyleSheet(
                QStringLiteral("QPushButton { background-color: %1; border: 1px solid #808080; }")
                        .arg(QString::fromStdString(first_opts->hex_color())));
    }
    _opengl_widget->update();
}

/**
 * @brief Apply slider alpha to all keys in the group
 */
void GroupIntervalViewer_Widget::_onAlphaChanged(int value) {
    if (_updating || _active_keys.empty()) {
        return;
    }
    float const alpha = static_cast<float>(value) / 100.0f;
    _alpha_value_label->setText(QString::number(static_cast<double>(alpha), 'f', 2));

    _applyToAllKeys([this, alpha](std::string const & key) {
        auto * mutable_opts =
                _opengl_widget->state()->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(
                        QString::fromStdString(key));
        if (mutable_opts) {
            mutable_opts->alpha() = alpha;
        }
    });
    _opengl_widget->update();
}

void GroupIntervalViewer_Widget::_onLayoutModeChanged(int index) {
    if (_updating || _active_keys.empty() || !_opengl_widget) {
        return;
    }
    bool const full_viewport = (index == 0);
    _applyToAllKeys([this, full_viewport](std::string const & key) {
        auto * mutable_opts =
                _opengl_widget->state()->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(
                        QString::fromStdString(key));
        if (mutable_opts) {
            mutable_opts->extend_full_canvas = full_viewport;
        }
    });
    _opengl_widget->updateCanvas();
}

void GroupIntervalViewer_Widget::_applyToAllKeys(std::function<void(std::string const &)> const & fn) {
    for (auto const & key: _active_keys) {
        fn(key);
    }
}
