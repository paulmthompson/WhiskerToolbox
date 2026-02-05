#include "ScatterPlotWidget.hpp"

#include "Core/ScatterPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Rendering/ScatterPlotOpenGLWidget.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_ScatterPlotWidget.h"

ScatterPlotWidget::ScatterPlotWidget(std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::ScatterPlotWidget),
      _opengl_widget(nullptr),
      _horizontal_axis_widget(nullptr),
      _vertical_axis_widget(nullptr)
{
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Create and add the vertical axis widget on the left
    _vertical_axis_widget = new VerticalAxisWidget(this);
    horizontal_layout->addWidget(_vertical_axis_widget);

    // Create and add the OpenGL widget
    _opengl_widget = new ScatterPlotOpenGLWidget(this);
    horizontal_layout->addWidget(_opengl_widget, 1);  // Stretch factor 1

    // Create vertical layout for horizontal layout + horizontal axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);  // Stretch factor 1

    // Create and add the horizontal axis widget below
    _horizontal_axis_widget = new HorizontalAxisWidget(this);
    vertical_layout->addWidget(_horizontal_axis_widget);

    // Replace the main layout
    QLayout * old_layout = layout();
    if (old_layout) {
        delete old_layout;
    }
    setLayout(vertical_layout);
}

ScatterPlotWidget::~ScatterPlotWidget()
{
    delete ui;
}

void ScatterPlotWidget::setState(std::shared_ptr<ScatterPlotState> state)
{
    _state = state;

    // Pass state to OpenGL widget
    if (_opengl_widget) {
        _opengl_widget->setState(_state);
    }

    // Set up horizontal axis widget with range getter from state
    if (_horizontal_axis_widget && _state) {
        _horizontal_axis_widget->setRangeGetter([this]() {
            if (!_state) {
                return std::make_pair(0.0, 100.0);
            }
            return std::make_pair(_state->getXMin(), _state->getXMax());
        });

        // Connect to X-axis range changes
        connect(_state.get(), &ScatterPlotState::xMinChanged,
                _horizontal_axis_widget, [this](double /* x_min */) {
                    if (_horizontal_axis_widget) {
                        _horizontal_axis_widget->update();
                    }
                });
        connect(_state.get(), &ScatterPlotState::xMaxChanged,
                _horizontal_axis_widget, [this](double /* x_max */) {
                    if (_horizontal_axis_widget) {
                        _horizontal_axis_widget->update();
                    }
                });

        // Initial update to show current state values
        _horizontal_axis_widget->update();
    }

    // Set up vertical axis widget with range getter from state
    if (_vertical_axis_widget && _state) {
        _vertical_axis_widget->setRangeGetter([this]() {
            if (!_state) {
                return std::make_pair(0.0, 100.0);
            }
            return std::make_pair(_state->getYMin(), _state->getYMax());
        });

        // Connect to Y-axis range changes
        connect(_state.get(), &ScatterPlotState::yMinChanged,
                _vertical_axis_widget, [this](double /* y_min */) {
                    if (_vertical_axis_widget) {
                        _vertical_axis_widget->update();
                    }
                });
        connect(_state.get(), &ScatterPlotState::yMaxChanged,
                _vertical_axis_widget, [this](double /* y_max */) {
                    if (_vertical_axis_widget) {
                        _vertical_axis_widget->update();
                    }
                });

        // Initial update to show current state values
        _vertical_axis_widget->update();
    }
}

ScatterPlotState * ScatterPlotWidget::state()
{
    return _state.get();
}

void ScatterPlotWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    // Update axis widgets when widget resizes to ensure they get fresh viewport dimensions
    if (_horizontal_axis_widget) {
        _horizontal_axis_widget->update();
    }
    if (_vertical_axis_widget) {
        _vertical_axis_widget->update();
    }
}
