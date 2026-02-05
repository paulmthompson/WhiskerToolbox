#ifndef PSTH_WIDGET_HPP
#define PSTH_WIDGET_HPP

/**
 * @file PSTHWidget.hpp
 * @brief Main widget for displaying PSTH plots
 * 
 * PSTHWidget displays Peri-Stimulus Time Histogram plots showing
 * event counts or rates aligned to a reference event.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QResizeEvent>
#include <QWidget>

#include <memory>

class DataManager;
class PSTHState;
class RelativeTimeAxisWidget;
class VerticalAxisWidget;
class PSTHPlotOpenGLWidget;

namespace Ui {
class PSTHWidget;
}

/**
 * @brief Main widget for PSTH plot visualization
 */
class PSTHWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a PSTHWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    PSTHWidget(std::shared_ptr<DataManager> data_manager,
               QWidget * parent = nullptr);

    ~PSTHWidget() override;

    /**
     * @brief Set the PSTHState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<PSTHState> state);

    /**
     * @brief Get the current PSTHState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<PSTHState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] PSTHState * state();

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    std::shared_ptr<DataManager> _data_manager;
    Ui::PSTHWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<PSTHState> _state;

    /// OpenGL rendering widget
    PSTHPlotOpenGLWidget * _opengl_widget;

    /// Time axis widget below the plot
    RelativeTimeAxisWidget * _axis_widget;

    /// Vertical axis widget on the left side
    VerticalAxisWidget * _vertical_axis_widget;
};

#endif// PSTH_WIDGET_HPP
