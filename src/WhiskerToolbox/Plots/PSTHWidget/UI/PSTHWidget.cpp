#include "PSTHWidget.hpp"

#include "Core/PSTHState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "DataManager/DataManager.hpp"
#include "Rendering/PSTHPlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"

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
      _range_controls(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr)
{
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Vertical axis widget and controls will be created in setState()
    // when we have access to the PSTHState's VerticalAxisState
    // For now, add a placeholder (will be replaced in setState)
    _vertical_axis_widget = nullptr;
    _vertical_range_controls = nullptr;

    // Create and add the OpenGL widget
    _opengl_widget = new PSTHPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);  // Stretch factor 1

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);  // Stretch factor 1

    // Time axis widget and controls will be created in setState()
    // when we have access to the PSTHState's RelativeTimeAxisState
    _axis_widget = nullptr;
    _range_controls = nullptr;

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

    // Create vertical axis widget and controls using factory
    // This must be done after setState so we have access to VerticalAxisState
    if (_state && !_vertical_axis_widget) {
        auto * vertical_axis_state = _state->verticalAxisState();
        if (vertical_axis_state) {
            auto vertical_axis_with_controls = createVerticalAxisWithRangeControls(
                vertical_axis_state, this, nullptr);
            _vertical_axis_widget = vertical_axis_with_controls.axis_widget;
            _vertical_range_controls = vertical_axis_with_controls.range_controls;

            // Add vertical axis widget to layout
            if (_vertical_axis_widget) {
                // Find the horizontal layout (should be the first child layout)
                QLayout * main_layout = layout();
                if (main_layout) {
                    QVBoxLayout * vbox = qobject_cast<QVBoxLayout *>(main_layout);
                    if (vbox && vbox->count() > 0) {
                        QLayoutItem * item = vbox->itemAt(0);
                        if (item && item->layout()) {
                            QHBoxLayout * hbox = qobject_cast<QHBoxLayout *>(item->layout());
                            if (hbox) {
                                // Insert at the beginning (before OpenGL widget)
                                hbox->insertWidget(0, _vertical_axis_widget);
                            }
                        }
                    }
                }
            }
        }
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

    // Time axis state syncing with window_size is handled in PSTHState
    // No additional syncing needed here

}

PSTHState * PSTHWidget::state()
{
    return _state.get();
}

RelativeTimeAxisRangeControls * PSTHWidget::getRangeControls() const
{
    return _range_controls;
}


VerticalAxisRangeControls * PSTHWidget::getVerticalRangeControls() const
{
    return _vertical_range_controls;
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
