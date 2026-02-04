#ifndef HEATMAP_WIDGET_HPP
#define HEATMAP_WIDGET_HPP

/**
 * @file HeatmapWidget.hpp
 * @brief Main widget for displaying heatmap plots
 * 
 * HeatmapWidget displays PSTH-style heatmap visualizations showing
 * data across multiple channels or trials.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class HeatmapState;

namespace Ui {
class HeatmapWidget;
}

/**
 * @brief Main widget for heatmap plot visualization
 */
class HeatmapWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a HeatmapWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    HeatmapWidget(std::shared_ptr<DataManager> data_manager,
                  QWidget * parent = nullptr);

    ~HeatmapWidget() override;

    /**
     * @brief Set the HeatmapState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<HeatmapState> state);

    /**
     * @brief Get the current HeatmapState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<HeatmapState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] HeatmapState * state();

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::HeatmapWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<HeatmapState> _state;
};

#endif// HEATMAP_WIDGET_HPP
