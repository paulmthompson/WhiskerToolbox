#ifndef TEMPORAL_PROJECTION_VIEW_WIDGET_HPP
#define TEMPORAL_PROJECTION_VIEW_WIDGET_HPP

/**
 * @file TemporalProjectionViewWidget.hpp
 * @brief Main widget for displaying temporal projection views
 * 
 * TemporalProjectionViewWidget displays 2D projections of data collapsed across temporal windows.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class TemporalProjectionViewState;

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

    /**
     * @brief Set the TemporalProjectionViewState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<TemporalProjectionViewState> state);

    /**
     * @brief Get the current TemporalProjectionViewState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<TemporalProjectionViewState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] TemporalProjectionViewState * state();

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::TemporalProjectionViewWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<TemporalProjectionViewState> _state;
};

#endif// TEMPORAL_PROJECTION_VIEW_WIDGET_HPP
