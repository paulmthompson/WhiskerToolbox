#include "PSTHWidget.hpp"

#include "Core/PSTHState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "DataManager/DataManager.hpp"
#include "Rendering/PSTHPlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_PSTHWidget.h"

PSTHWidget::PSTHWidget(std::shared_ptr<DataManager> data_manager,
                       QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::PSTHWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr),
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
    _opengl_widget = new PSTHPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);  // Stretch factor 1

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);  // Stretch factor 1

    // Create and add the time axis widget below
    _axis_widget = new RelativeTimeAxisWidget(this);
    vertical_layout->addWidget(_axis_widget);

    // Replace the main layout
    QLayout * old_layout = layout();
    if (old_layout) {
        delete old_layout;
    }
    setLayout(vertical_layout);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &PSTHPlotOpenGLWidget::plotDoubleClicked,
            this, [this](int64_t time_frame_index) {
                emit timePositionSelected(TimePosition(time_frame_index));
            });
}

PSTHWidget::~PSTHWidget()
{
    delete ui;
}

void PSTHWidget::setState(std::shared_ptr<PSTHState> state)
{
    _state = state;
    
    // Pass state to OpenGL widget
    if (_opengl_widget) {
        _opengl_widget->setState(_state);
    }

    // Set up axis widget with ViewState getter and connect to state changes
    if (_axis_widget && _state) {
        // Set up the ViewState getter function
        _axis_widget->setViewStateGetter([this]() {
            if (!_state || !_opengl_widget) {
                return CorePlotting::ViewState{};
            }
            // Get viewport size from OpenGL widget
            int width = _opengl_widget->width();
            int height = _opengl_widget->height();
            return toCoreViewState(_state.get(), width, height);
        });

        // Connect to window size changes (emitted when window size spinbox is adjusted)
        // Use regular connect since windowSizeChanged has a parameter
        connect(_state.get(), &PSTHState::windowSizeChanged,
                _axis_widget, [this](double /* window_size */) {
                    if (_axis_widget) {
                        _axis_widget->update();
                    }
                });

        // Also connect to OpenGL widget's viewBoundsChanged signal
        // This ensures the axis widget updates when the OpenGL widget's view changes
        connect(_opengl_widget, &PSTHPlotOpenGLWidget::viewBoundsChanged,
                _axis_widget, [this]() {
                    if (_axis_widget) {
                        _axis_widget->update();
                    }
                });
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
        connect(_state.get(), &PSTHState::yMinChanged,
                _vertical_axis_widget, [this](double /* y_min */) {
                    if (_vertical_axis_widget) {
                        _vertical_axis_widget->update();
                    }
                });
        connect(_state.get(), &PSTHState::yMaxChanged,
                _vertical_axis_widget, [this](double /* y_max */) {
                    if (_vertical_axis_widget) {
                        _vertical_axis_widget->update();
                    }
                });

        // Initial update to show current state values
        _vertical_axis_widget->update();
    }
}

PSTHState * PSTHWidget::state()
{
    return _state.get();
}

void PSTHWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    // Update axis widgets when widget resizes to ensure they get fresh viewport dimensions
    if (_axis_widget) {
        _axis_widget->update();
    }
    if (_vertical_axis_widget) {
        _vertical_axis_widget->update();
    }
}
