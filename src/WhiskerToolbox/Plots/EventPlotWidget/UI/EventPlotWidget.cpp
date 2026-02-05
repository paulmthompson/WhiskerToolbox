#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "Rendering/EventPlotOpenGLWidget.hpp"
#include "EventPlotAxisWidget.hpp"

#include <QVBoxLayout>

#include "ui_EventPlotWidget.h"

EventPlotWidget::EventPlotWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::EventPlotWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr)
{
    ui->setupUi(this);

    // Create and add the OpenGL widget
    _opengl_widget = new EventPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    ui->main_layout->addWidget(_opengl_widget);

    // Create and add the axis widget below the OpenGL canvas
    _axis_widget = new EventPlotAxisWidget(this);
    ui->main_layout->addWidget(_axis_widget);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &EventPlotOpenGLWidget::eventDoubleClicked,
            this, [this](int64_t time_frame_index, QString const & /* series_key */) {
                emit timePositionSelected(TimePosition(time_frame_index));
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

    // Pass state to axis widget
    if (_axis_widget) {
        _axis_widget->setState(_state);
    }
}

EventPlotState * EventPlotWidget::state()
{
    return _state.get();
}
