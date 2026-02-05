#ifndef ACF_WIDGET_HPP
#define ACF_WIDGET_HPP

/**
 * @file ACFWidget.hpp
 * @brief Main widget for displaying autocorrelation function plots
 * 
 * ACFWidget displays autocorrelation functions computed from DigitalEventSeries data.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class ACFState;

namespace Ui {
class ACFWidget;
}

/**
 * @brief Main widget for autocorrelation function visualization
 */
class ACFWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct an ACFWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    ACFWidget(std::shared_ptr<DataManager> data_manager,
              QWidget * parent = nullptr);

    ~ACFWidget() override;

    /**
     * @brief Set the ACFState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<ACFState> state);

    /**
     * @brief Get the current ACFState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<ACFState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] ACFState * state();

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::ACFWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<ACFState> _state;
};

#endif// ACF_WIDGET_HPP
