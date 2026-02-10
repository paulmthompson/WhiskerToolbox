#ifndef VERTICAL_AXIS_SYNCHRONIZER_HPP
#define VERTICAL_AXIS_SYNCHRONIZER_HPP

/**
 * @file VerticalAxisSynchronizer.hpp
 * @brief Helper function for synchronizing VerticalAxisState with plot ViewState
 * 
 * This helper encapsulates the "Silent Update" logic for bidirectional synchronization
 * between plot ViewState (from panning/zooming) and VerticalAxisState (for spinbox controls).
 * 
 * When ViewState changes (Flow B: OpenGL Pan/Zoom), this updates the AxisState silently
 * using setRangeSilent, which emits rangeUpdated (for UI refresh) but NOT rangeChanged
 * (preventing feedback loops).
 * 
 * @see The "Golden Cycle" of Synchronization in the Axis Synchronization & Widget API Guide
 */

#include "Core/VerticalAxisState.hpp"

#include <QObject>

#include <functional>
#include <utility>

/**
 * @brief Synchronize VerticalAxisState with plot ViewState changes
 * 
 * This function sets up a connection from the plot state's viewStateChanged signal
 * to update the vertical axis state silently when the view changes (e.g., from panning/zooming).
 * 
 * The compute_bounds function should calculate the visible range from the ViewState.
 * This visible range is then set on the axis state using setRangeSilent, which updates
 * the spinboxes without triggering rangeChanged (avoiding feedback loops).
 * 
 * @tparam StateType Type of the plot state class. Must have:
 *   - A method `ViewStateType const& viewState() const` that returns the current view state
 *   - A signal `void viewStateChanged()` that is emitted when the view state changes
 * @tparam ComputeBoundsType Type of the compute_bounds callable (deduced automatically)
 * 
 * @param axis_state Pointer to the VerticalAxisState to synchronize
 * @param plot_state Pointer to the plot state object
 * @param compute_bounds Callable that takes a ViewState and returns (min, max) pair
 * 
 * @pre axis_state != nullptr
 * @pre plot_state != nullptr
 * @pre compute_bounds is a valid callable
 * 
 * @post When plot_state emits viewStateChanged(), axis_state will be updated silently
 *       with the computed bounds from the current view state
 */
template <typename StateType, typename ComputeBoundsType>
void syncVerticalAxisToViewState(
    VerticalAxisState * axis_state,
    StateType * plot_state,
    ComputeBoundsType compute_bounds)
{
    if (!axis_state || !plot_state) {
        return;
    }

    // When ViewState changes (Pan/Zoom), update the Axis State silently
    QObject::connect(plot_state, &StateType::viewStateChanged, axis_state,
                     [axis_state, plot_state, compute_bounds]() {
                         auto bounds = compute_bounds(plot_state->viewState());

                         // Critical: setRangeSilent updates the data and emits rangeUpdated (for UI)
                         // but DOES NOT emit rangeChanged (preventing loops)
                         axis_state->setRangeSilent(bounds.first, bounds.second);
                     });
}

#endif  // VERTICAL_AXIS_SYNCHRONIZER_HPP
