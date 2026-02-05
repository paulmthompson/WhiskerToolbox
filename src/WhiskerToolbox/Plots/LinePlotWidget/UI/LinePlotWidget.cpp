#include "LinePlotWidget.hpp"

#include "Core/LinePlotState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "DataManager/DataManager.hpp"
#include "Rendering/LinePlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"

#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_LinePlotWidget.h"

LinePlotWidget::LinePlotWidget(std::shared_ptr<DataManager> data_manager,
                                QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::LinePlotWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr)
{
    ui->setupUi(this);

    // Create and add the OpenGL widget
    _opengl_widget = new LinePlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    ui->main_layout->addWidget(_opengl_widget);

    // Create and add the axis widget below the OpenGL canvas
    _axis_widget = new RelativeTimeAxisWidget(this);
    ui->main_layout->addWidget(_axis_widget);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &LinePlotOpenGLWidget::plotDoubleClicked,
            this, [this](int64_t time_frame_index) {
                emit timePositionSelected(TimePosition(time_frame_index));
            });
}

LinePlotWidget::~LinePlotWidget()
{
    delete ui;
}

void LinePlotWidget::setState(std::shared_ptr<LinePlotState> state)
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
        connect(_state.get(), &LinePlotState::windowSizeChanged,
                _axis_widget, [this](double /* window_size */) {
                    if (_axis_widget) {
                        _axis_widget->update();
                    }
                });

        // Also connect to OpenGL widget's viewBoundsChanged signal
        // This ensures the axis widget updates when the OpenGL widget's view changes
        connect(_opengl_widget, &LinePlotOpenGLWidget::viewBoundsChanged,
                _axis_widget, [this]() {
                    if (_axis_widget) {
                        _axis_widget->update();
                    }
                });
    }
}

LinePlotState * LinePlotWidget::state()
{
    return _state.get();
}

void LinePlotWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    // Update axis widget when widget resizes to ensure it gets fresh viewport dimensions
    if (_axis_widget) {
        _axis_widget->update();
    }
}
