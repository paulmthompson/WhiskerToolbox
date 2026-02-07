#ifndef HEATMAP_WIDGET_HPP
#define HEATMAP_WIDGET_HPP

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>
#include <utility>

class DataManager;
class RelativeTimeAxisWidget;
class RelativeTimeAxisRangeControls;
class VerticalAxisWidget;
class VerticalAxisRangeControls;
class VerticalAxisState;
class HeatmapOpenGLWidget;
class HeatmapState;

namespace Ui {
class HeatmapWidget;
}

class HeatmapWidget : public QWidget {
    Q_OBJECT

public:
    HeatmapWidget(std::shared_ptr<DataManager> data_manager,
                  QWidget * parent = nullptr);
    ~HeatmapWidget() override;

    void setState(std::shared_ptr<HeatmapState> state);
    [[nodiscard]] std::shared_ptr<HeatmapState> state() const { return _state; }
    [[nodiscard]] HeatmapState * state();
    [[nodiscard]] RelativeTimeAxisRangeControls * getRangeControls() const;
    [[nodiscard]] VerticalAxisRangeControls * getVerticalRangeControls() const;
    [[nodiscard]] VerticalAxisState * getVerticalAxisState() const;

signals:
    void timePositionSelected(TimePosition position);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    // -- setState decomposition --
    void createTimeAxisIfNeeded();
    void wireTimeAxis();
    void wireVerticalAxis();
    void connectViewChangeSignals();
    void syncTimeAxisRange();
    void syncVerticalAxisRange();

    // -- Visible range helpers --
    [[nodiscard]] std::pair<double, double> computeVisibleTrialRange() const;
    [[nodiscard]] std::pair<double, double> computeVisibleTimeRange() const;

    std::shared_ptr<DataManager> _data_manager;
    Ui::HeatmapWidget * ui;
    std::shared_ptr<HeatmapState> _state;
    HeatmapOpenGLWidget * _opengl_widget;
    RelativeTimeAxisWidget * _axis_widget;
    RelativeTimeAxisRangeControls * _range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
    std::unique_ptr<VerticalAxisState> _vertical_axis_state;
    size_t _trial_count = 0;
};

#endif// HEATMAP_WIDGET_HPP
