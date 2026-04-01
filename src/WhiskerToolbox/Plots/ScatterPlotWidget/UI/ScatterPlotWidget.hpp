#ifndef SCATTER_PLOT_WIDGET_HPP
#define SCATTER_PLOT_WIDGET_HPP

/**
 * @file ScatterPlotWidget.hpp
 * @brief Main widget for displaying 2D scatter plots
 *
 * Single source of truth: ScatterPlotState. Horizontal and vertical axis
 * widgets use state; pan/zoom in OpenGL widget update state.
 */

#include "DataTypeEnum/DM_DataType.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QWidget>

#include <memory>

class DataManager;
class GroupManager;
class ScatterPlotState;
class ScatterPlotOpenGLWidget;
class SelectionContext;
class HorizontalAxisRangeControls;
class HorizontalAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisWidget;

class QResizeEvent;

namespace KeymapSystem {
class KeymapManager;
}// namespace KeymapSystem

namespace Ui {
class ScatterPlotWidget;
}

/**
 * @brief Main widget for 2D scatter plot visualization
 */
class ScatterPlotWidget : public QWidget {
    Q_OBJECT

public:
    ScatterPlotWidget(std::shared_ptr<DataManager> data_manager,
                      QWidget * parent = nullptr);
    ~ScatterPlotWidget() override;

    void setState(std::shared_ptr<ScatterPlotState> state);
    [[nodiscard]] std::shared_ptr<ScatterPlotState> state() const { return _state; }
    [[nodiscard]] ScatterPlotState * state();

    /**
     * @brief Set the GroupManager for group-related context menu actions
     * @param group_manager Pointer to the GroupManager (not owned)
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Set the SelectionContext for ContextAction integration
     * @param selection_context Pointer to the SelectionContext (not owned)
     */
    void setSelectionContext(SelectionContext * selection_context);

    /**
     * @brief Set the KeymapManager to enable configurable polygon editing shortcuts
     * @param manager Pointer to the KeymapManager (can be nullptr)
     */
    void setKeymapManager(KeymapSystem::KeymapManager * manager);

    [[nodiscard]] HorizontalAxisRangeControls * getHorizontalRangeControls() const;
    [[nodiscard]] VerticalAxisRangeControls * getVerticalRangeControls() const;

signals:
    void timePositionSelected(TimePosition position);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    void createHorizontalAxisIfNeeded();
    void createVerticalAxisIfNeeded();
    void wireHorizontalAxis();
    void wireVerticalAxis();
    void connectViewChangeSignals();
    void syncHorizontalAxisRange();
    void syncVerticalAxisRange();

    [[nodiscard]] std::pair<double, double> computeVisibleXRange() const;
    [[nodiscard]] std::pair<double, double> computeVisibleYRange() const;

    std::shared_ptr<DataManager> _data_manager;
    Ui::ScatterPlotWidget * ui;
    std::shared_ptr<ScatterPlotState> _state;
    ScatterPlotOpenGLWidget * _opengl_widget;

    HorizontalAxisWidget * _horizontal_axis_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
};

#endif// SCATTER_PLOT_WIDGET_HPP
