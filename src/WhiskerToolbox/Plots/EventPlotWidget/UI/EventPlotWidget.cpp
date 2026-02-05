#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "DataManager/DataManager.hpp"
#include "Rendering/EventPlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_EventPlotWidget.h"

EventPlotWidget::EventPlotWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::EventPlotWidget),
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
    _vertical_axis_widget->setRange(0.0, 0.0);  // Will be updated when trials are loaded
    horizontal_layout->addWidget(_vertical_axis_widget);

    // Create and add the OpenGL widget
    _opengl_widget = new EventPlotOpenGLWidget(this);
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
    connect(_opengl_widget, &EventPlotOpenGLWidget::eventDoubleClicked,
            this, [this](int64_t time_frame_index, QString const & /* series_key */) {
                emit timePositionSelected(TimePosition(time_frame_index));
            });

    // Connect trial count changes to update vertical axis
    connect(_opengl_widget, &EventPlotOpenGLWidget::trialCountChanged,
            this, [this](size_t count) {
                if (_vertical_axis_widget) {
                    _vertical_axis_widget->setRange(0.0, static_cast<double>(count));
                }
            });
}

EventPlotWidget::~EventPlotWidget()
{
    delete ui;
}

void EventPlotWidget::setState(std::shared_ptr<EventPlotState> state)
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
            auto const & event_view_state = _state->viewState();
            // Get viewport size from OpenGL widget
            int width = _opengl_widget->width();
            int height = _opengl_widget->height();
            return toCoreViewState(event_view_state, width, height);
        });

        // Connect to view state changes (emitted when window size, zoom, pan, or bounds change)
        _axis_widget->connectToViewStateChanged(
            _state.get(),
            &EventPlotState::viewStateChanged);

        // Also connect to OpenGL widget's viewBoundsChanged signal
        // This ensures the axis widget updates when the OpenGL widget's view changes
        connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
                _axis_widget, [this]() {
                    if (_axis_widget) {
                        _axis_widget->update();
                    }
                });
    }

    // Update vertical axis when widget resizes
    if (_vertical_axis_widget) {
        _vertical_axis_widget->update();
    }
}

void EventPlotWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    // Update axis widget when widget resizes to ensure it gets fresh viewport dimensions
    if (_axis_widget) {
        _axis_widget->update();
    }
}

EventPlotState * EventPlotWidget::state()
{
    return _state.get();
}
