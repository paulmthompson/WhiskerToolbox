#ifndef THREE_D_PLOT_WIDGET_HPP
#define THREE_D_PLOT_WIDGET_HPP

/**
 * @file 3DPlotWidget.hpp
 * @brief Main widget for displaying 3D plots
 * 
 * 3DPlotWidget displays 3D visualizations of data.
 */

#include "DataManager/DataManagerFwd.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class ThreeDPlotState;
class ThreeDPlotOpenGLWidget;

namespace Ui {
class ThreeDPlotWidget;
}

/**
 * @brief Main widget for 3D plot visualization
 */
class ThreeDPlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a ThreeDPlotWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    ThreeDPlotWidget(std::shared_ptr<DataManager> data_manager,
                     QWidget * parent = nullptr);

    ~ThreeDPlotWidget() override;

    /**
     * @brief Set the ThreeDPlotState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<ThreeDPlotState> state);

    /**
     * @brief Get the current ThreeDPlotState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<ThreeDPlotState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] ThreeDPlotState * state();

    /**
     * @brief Handle time changes from EditorRegistry
     * 
     * Updates the plot when time changes come from other sources
     * (e.g., user scrubs through time via TimeScrollBar).
     * 
     * @param position The new TimePosition
     */
    void _onTimeChanged(TimePosition position);

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
    Ui::ThreeDPlotWidget * ui;

    /// Serializable state shared with properties widget
    std::shared_ptr<ThreeDPlotState> _state;

    /// OpenGL widget for 3D rendering
    ThreeDPlotOpenGLWidget * _opengl_widget{nullptr};
};

#endif// THREE_D_PLOT_WIDGET_HPP
