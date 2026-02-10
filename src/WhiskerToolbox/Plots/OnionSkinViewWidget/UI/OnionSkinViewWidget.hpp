#ifndef ONION_SKIN_VIEW_WIDGET_HPP
#define ONION_SKIN_VIEW_WIDGET_HPP

/**
 * @file OnionSkinViewWidget.hpp
 * @brief Main widget for displaying onion skin views
 *
 * OnionSkinViewWidget displays onion skin views of data. Single source of truth: OnionSkinViewState.
 * Horizontal and vertical axis widgets use state; pan/zoom in OpenGL widget
 * update state.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QWidget>

#include <memory>

class DataManager;
class OnionSkinViewState;
class OnionSkinViewOpenGLWidget;
class HorizontalAxisRangeControls;
class HorizontalAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisWidget;

class QResizeEvent;

namespace Ui {
class OnionSkinViewWidget;
}

/**
 * @brief Main widget for onion skin view visualization
 */
class OnionSkinViewWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a OnionSkinViewWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    OnionSkinViewWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent = nullptr);

    ~OnionSkinViewWidget() override;

    void setState(std::shared_ptr<OnionSkinViewState> state);
    [[nodiscard]] std::shared_ptr<OnionSkinViewState> state() const { return _state; }
    [[nodiscard]] OnionSkinViewState * state();

    [[nodiscard]] HorizontalAxisRangeControls * getHorizontalRangeControls() const;
    [[nodiscard]] VerticalAxisRangeControls * getVerticalRangeControls() const;

    /**
     * @brief Handle time changes from EditorRegistry
     *
     * Slot for global time changes (e.g. TimeScrollBar). Can be used to update
     * the view when time changes from other sources.
     *
     * @param position The new TimePosition
     */
    void _onTimeChanged(TimePosition position);

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
    Ui::OnionSkinViewWidget * ui;
    std::shared_ptr<OnionSkinViewState> _state;
    OnionSkinViewOpenGLWidget * _opengl_widget;

    HorizontalAxisWidget * _horizontal_axis_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
};

#endif  // ONION_SKIN_VIEW_WIDGET_HPP
