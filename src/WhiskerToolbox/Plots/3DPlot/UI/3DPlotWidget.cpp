#include "3DPlotWidget.hpp"

#include "Core/3DPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Rendering/ThreeDPlotOpenGLWidget.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_3DPlotWidget.h"

ThreeDPlotWidget::ThreeDPlotWidget(std::shared_ptr<DataManager> data_manager,
                                   QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::ThreeDPlotWidget),
      _state(nullptr) {
    ui->setupUi(this);

    // Create OpenGL widget for 3D rendering
    _opengl_widget = new ThreeDPlotOpenGLWidget(this);

    // Add OpenGL widget to the center of the layout
    ui->main_layout->addWidget(_opengl_widget);
}

ThreeDPlotWidget::~ThreeDPlotWidget() {
    delete ui;
}

void ThreeDPlotWidget::setState(std::shared_ptr<ThreeDPlotState> state) {
    _state = state;

    // Pass state to OpenGL widget
    if (_opengl_widget && _state) {
        _opengl_widget->setState(_state);
    }
}

void ThreeDPlotWidget::_onTimeChanged(TimePosition position) {
    if (!_opengl_widget || !_state || !_data_manager) {
        return;
    }

    // Get all plot data keys from state
    auto data_keys = _state->getPlotDataKeys();
    if (data_keys.empty()) {
        // No data keys added, clear the display
        _opengl_widget->updateTime(_data_manager, position);
        return;
    }

    // Use the first data key to determine the TimeFrame
    // (assuming all keys share the same TimeFrame - could be improved)
    QString first_key_qstr = data_keys[0];
    std::string first_key = first_key_qstr.toStdString();

    // Get the TimeFrame for the PointData
    auto point_data = _data_manager->getData<PointData>(first_key);
    if (!point_data) {
        return;  // PointData doesn't exist
    }

    auto time_key = _data_manager->getTimeKey(first_key);
    auto my_tf = _data_manager->getTime(time_key);

    if (!my_tf) {
        return;  // No TimeFrame available
    }

    // Convert time position if needed
    TimePosition converted_position;
    if (position.sameClock(my_tf)) {
        // Same clock - use position directly
        converted_position = position;
    } else if (position.isValid()) {
        // Different clock - convert
        auto converted_index = position.convertTo(my_tf);
        converted_position = TimePosition(converted_index, my_tf);
    } else {
        return;  // Invalid position
    }

    // Update the OpenGL widget with the new time position (it will handle all keys)
    _opengl_widget->updateTime(_data_manager, converted_position);
}

void ThreeDPlotWidget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    // Handle resize if needed
}

ThreeDPlotState * ThreeDPlotWidget::state() {
    return _state.get();
}
