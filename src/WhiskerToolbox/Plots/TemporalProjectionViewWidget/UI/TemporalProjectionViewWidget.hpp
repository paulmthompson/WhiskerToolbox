#ifndef TEMPORAL_PROJECTION_VIEW_WIDGET_HPP
#define TEMPORAL_PROJECTION_VIEW_WIDGET_HPP

/**
 * @file TemporalProjectionViewWidget.hpp
 * @brief Main widget for displaying temporal projection views
 *
 * TemporalProjectionViewWidget displays 2D projections of data collapsed across
 * temporal windows. Single source of truth: TemporalProjectionViewState.
 * Horizontal and vertical axis widgets use state; pan/zoom in OpenGL widget
 * update state.
 */

#include "DataTypeEnum/DM_DataType.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QWidget>

#include <memory>

class DataManager;
class GroupManager;
class TemporalProjectionViewState;
class TemporalProjectionOpenGLWidget;
class HorizontalAxisRangeControls;
class HorizontalAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisWidget;

class QResizeEvent;

namespace Ui {
class TemporalProjectionViewWidget;
}

/**
 * @brief Main widget for temporal projection view visualization
 */
class TemporalProjectionViewWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a TemporalProjectionViewWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    TemporalProjectionViewWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent = nullptr);

    ~TemporalProjectionViewWidget() override;

    void setState(std::shared_ptr<TemporalProjectionViewState> state);
    [[nodiscard]] std::shared_ptr<TemporalProjectionViewState> state() const { return _state; }
    [[nodiscard]] TemporalProjectionViewState * state();

    [[nodiscard]] HorizontalAxisRangeControls * getHorizontalRangeControls() const;
    [[nodiscard]] VerticalAxisRangeControls * getVerticalRangeControls() const;

    /**
     * @brief Clear all entity selections in the OpenGL widget
     */
    void clearSelection();

    /**
     * @brief Set the GroupManager for group-related context menu actions
     * @param group_manager Pointer to the GroupManager (not owned)
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Handle time changes from EditorRegistry
     *
     * Slot for global time changes (e.g. TimeScrollBar). Can be used to update
     * the view when time changes from other sources.
     *
     * @param position The new TimePosition
     */
    void _onTimeChanged(const TimePosition& position);

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

    /// Check if any visualized keys have been removed from DataManager and clear them
    void _pruneRemovedKeys();

    std::shared_ptr<DataManager> _data_manager;
    Ui::TemporalProjectionViewWidget * ui;
    std::shared_ptr<TemporalProjectionViewState> _state;
    TemporalProjectionOpenGLWidget * _opengl_widget;

    HorizontalAxisWidget * _horizontal_axis_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;

    /// DataManager-level observer ID for detecting key additions/removals
    int _dm_observer_id{-1};
};

#endif// TEMPORAL_PROJECTION_VIEW_WIDGET_HPP
