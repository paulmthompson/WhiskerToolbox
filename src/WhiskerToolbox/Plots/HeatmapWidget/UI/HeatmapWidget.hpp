#ifndef HEATMAP_WIDGET_HPP
#define HEATMAP_WIDGET_HPP

#include "DataTypeEnum/DM_DataType.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>
#include <utility>

class DataManager;
class RelativeTimeAxisWidget;
class RelativeTimeAxisRangeControls;
class SelectionContext;
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
    /** @brief Vertical axis state is owned by HeatmapState */
    [[nodiscard]] VerticalAxisState * getVerticalAxisState() const;

    /**
     * @brief Set the SelectionContext for ContextAction integration in the context menu
     * @param selection_context Pointer to the SelectionContext (not owned)
     */
    void setSelectionContext(SelectionContext * selection_context);

signals:
    void timePositionSelected(TimePosition position);

public slots:
    /**
     * @brief Open a file dialog and write the current heatmap scene to an SVG file.
     *
     * Uses `AppFileDialog` with dialog id `"export_heatmap_svg"`. Shows a message box on
     * success or failure. Does nothing if the scene is empty or the user cancels.
     */
    void handleExportSVG();

    /**
     * @brief Open a file dialog and write aggregate heatmap data to a CSV file.
     *
     * Uses `AppFileDialog` with dialog id `"export_heatmap_csv"`. Re-runs the
     * heatmap pipeline and calls `PlotDataExport::exportHeatmapToCSV()`.
     */
    void handleExportCSV();

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
    [[nodiscard]] std::pair<double, double> computeVisibleUnitRange() const;
    [[nodiscard]] std::pair<double, double> computeVisibleTimeRange() const;

    std::shared_ptr<DataManager> _data_manager;
    Ui::HeatmapWidget * ui;
    std::shared_ptr<HeatmapState> _state;
    HeatmapOpenGLWidget * _opengl_widget;
    RelativeTimeAxisWidget * _axis_widget;
    RelativeTimeAxisRangeControls * _range_controls;
    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;
    size_t _unit_count = 0;
};

#endif// HEATMAP_WIDGET_HPP
