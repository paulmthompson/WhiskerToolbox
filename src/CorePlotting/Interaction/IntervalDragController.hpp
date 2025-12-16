#ifndef COREPLOTTING_INTERACTION_INTERVALDRAGCONTROLLER_HPP
#define COREPLOTTING_INTERACTION_INTERVALDRAGCONTROLLER_HPP

#include "HitTestResult.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace CorePlotting {

/**
 * @brief Which edge of an interval is being dragged
 */
enum class DraggedEdge {
    None,   ///< No drag in progress
    Left,   ///< Dragging left (start) edge
    Right   ///< Dragging right (end) edge
};

/**
 * @brief State of an interval drag operation
 * 
 * Captures all information needed to track and constrain an interval edge
 * drag operation. The state is updated during mouse move events and
 * applied when the drag is completed.
 */
struct IntervalDragState {
    std::string series_key;           ///< Series containing the interval
    EntityId entity_id{0};            ///< EntityId of the interval being dragged
    DraggedEdge edge{DraggedEdge::None}; ///< Which edge is being dragged
    
    int64_t original_start{0};        ///< Original start time before drag
    int64_t original_end{0};          ///< Original end time before drag
    int64_t current_start{0};         ///< Current start time during drag
    int64_t current_end{0};           ///< Current end time during drag
    
    float drag_start_x{0.0f};         ///< World X where drag started
    
    bool is_active{false};            ///< Whether a drag is in progress
    
    /**
     * @brief Get the currently proposed interval bounds
     * @return Pair of (start, end) times
     */
    [[nodiscard]] std::pair<int64_t, int64_t> getCurrentBounds() const {
        return {current_start, current_end};
    }
    
    /**
     * @brief Get the original interval bounds
     * @return Pair of (start, end) times
     */
    [[nodiscard]] std::pair<int64_t, int64_t> getOriginalBounds() const {
        return {original_start, original_end};
    }
    
    /**
     * @brief Check if the interval has been modified
     */
    [[nodiscard]] bool hasChanged() const {
        return current_start != original_start || current_end != original_end;
    }
};

/**
 * @brief Configuration for interval drag behavior
 */
struct IntervalDragConfig {
    /// Minimum interval width in time units
    int64_t min_width{1};
    
    /// Maximum interval width in time units (0 = no limit)
    int64_t max_width{0};
    
    /// Minimum allowed start time (0 = no limit)
    int64_t min_time{0};
    
    /// Maximum allowed end time (0 = no limit)
    int64_t max_time{0};
    
    /// Whether to snap to integer time values
    bool snap_to_integer{true};
    
    /// Whether dragging past the opposite edge swaps which edge is being dragged
    bool allow_edge_swap{false};
};

/**
 * @brief State machine controller for interval edge dragging
 * 
 * This class encapsulates the logic for dragging interval edges, including:
 * - Starting a drag from a hit test result
 * - Updating the proposed bounds as the mouse moves
 * - Enforcing constraints (min width, bounds, etc.)
 * - Canceling or completing the drag
 * 
 * The controller is Qt-independent and works entirely with world coordinates.
 * Widgets use this controller by:
 * 1. Calling `startDrag()` when a mouse press hits an interval edge
 * 2. Calling `updateDrag()` on mouse move events
 * 3. Calling `finishDrag()` on mouse release
 * 
 * Example:
 * @code
 * IntervalDragController controller;
 * 
 * // On mouse press
 * if (hit_result.isIntervalEdge()) {
 *     controller.startDrag(hit_result);
 * }
 * 
 * // On mouse move
 * if (controller.isActive()) {
 *     controller.updateDrag(world_x);
 *     // Redraw to show preview of dragged interval
 *     float preview_start = controller.getState().current_start;
 *     float preview_end = controller.getState().current_end;
 * }
 * 
 * // On mouse release
 * if (controller.isActive()) {
 *     auto final_state = controller.finishDrag();
 *     if (final_state.hasChanged()) {
 *         applyIntervalChange(final_state);
 *     }
 * }
 * @endcode
 */
class IntervalDragController {
public:
    /**
     * @brief Construct with default configuration
     */
    IntervalDragController() = default;
    
    /**
     * @brief Construct with custom configuration
     */
    explicit IntervalDragController(IntervalDragConfig config)
        : _config(std::move(config)) {}
    
    /**
     * @brief Set drag configuration
     */
    void setConfig(IntervalDragConfig config) {
        _config = std::move(config);
    }
    
    /**
     * @brief Get current configuration
     */
    [[nodiscard]] IntervalDragConfig const& getConfig() const {
        return _config;
    }
    
    /**
     * @brief Start an interval edge drag operation
     * 
     * Initializes the drag state from a hit test result. Only succeeds
     * if the hit is on an interval edge.
     * 
     * @param hit_result Result from SceneHitTester::hitTest() or findIntervalEdge()
     * @return true if drag was started, false if hit is not on an interval edge
     */
    bool startDrag(HitTestResult const& hit_result);
    
    /**
     * @brief Update drag with new mouse position
     * 
     * Recalculates the proposed interval bounds based on the new world X
     * coordinate. Constraints are applied to ensure valid bounds.
     * 
     * @param world_x Current world X coordinate of mouse
     * @return true if bounds changed, false if no change (e.g., at constraint limit)
     */
    bool updateDrag(float world_x);
    
    /**
     * @brief Complete the drag operation
     * 
     * Returns the final drag state and resets the controller.
     * The caller should apply the changes to the actual data if hasChanged() is true.
     * 
     * @return Final drag state with proposed bounds
     */
    IntervalDragState finishDrag();
    
    /**
     * @brief Cancel the drag operation without applying changes
     * 
     * Resets the controller to inactive state. The original bounds are
     * available in the returned state for restoration if needed.
     * 
     * @return Drag state with original bounds (for restoration)
     */
    IntervalDragState cancelDrag();
    
    /**
     * @brief Check if a drag is currently active
     */
    [[nodiscard]] bool isActive() const {
        return _state.is_active;
    }
    
    /**
     * @brief Get current drag state
     * 
     * Use this to render a preview of the dragged interval.
     */
    [[nodiscard]] IntervalDragState const& getState() const {
        return _state;
    }

private:
    IntervalDragConfig _config;
    IntervalDragState _state;
    
    /**
     * @brief Apply constraints to proposed bounds
     */
    void enforceConstraints();
    
    /**
     * @brief Reset state to inactive
     */
    void reset();
};

// ============================================================================
// Implementation
// ============================================================================

inline bool IntervalDragController::startDrag(HitTestResult const& hit_result) {
    // Only start drag for interval edge hits
    if (!hit_result.isIntervalEdge()) {
        return false;
    }
    
    // Must have interval bounds
    if (!hit_result.interval_start.has_value() || !hit_result.interval_end.has_value()) {
        return false;
    }
    
    _state = IntervalDragState{};
    _state.series_key = hit_result.series_key;
    _state.entity_id = hit_result.entity_id.value_or(EntityId{0});
    _state.edge = (hit_result.hit_type == HitType::IntervalEdgeLeft) 
                  ? DraggedEdge::Left 
                  : DraggedEdge::Right;
    
    _state.original_start = hit_result.interval_start.value();
    _state.original_end = hit_result.interval_end.value();
    _state.current_start = _state.original_start;
    _state.current_end = _state.original_end;
    _state.drag_start_x = hit_result.world_x;
    _state.is_active = true;
    
    return true;
}

inline bool IntervalDragController::updateDrag(float world_x) {
    if (!_state.is_active) {
        return false;
    }
    
    int64_t old_start = _state.current_start;
    int64_t old_end = _state.current_end;
    
    // Convert to integer time if snapping
    int64_t new_time = static_cast<int64_t>(
        _config.snap_to_integer ? std::round(world_x) : world_x
    );
    
    if (_state.edge == DraggedEdge::Left) {
        _state.current_start = new_time;
    } else {
        _state.current_end = new_time;
    }
    
    // Apply constraints
    enforceConstraints();
    
    return _state.current_start != old_start || _state.current_end != old_end;
}

inline IntervalDragState IntervalDragController::finishDrag() {
    IntervalDragState result = _state;
    reset();
    return result;
}

inline IntervalDragState IntervalDragController::cancelDrag() {
    IntervalDragState result = _state;
    // Restore original bounds in the result for UI restoration
    result.current_start = result.original_start;
    result.current_end = result.original_end;
    reset();
    return result;
}

inline void IntervalDragController::enforceConstraints() {
    // Ensure start <= end (handle edge swap if allowed)
    if (_state.current_start > _state.current_end) {
        if (_config.allow_edge_swap) {
            // Swap which edge we're dragging
            std::swap(_state.current_start, _state.current_end);
            _state.edge = (_state.edge == DraggedEdge::Left) 
                         ? DraggedEdge::Right 
                         : DraggedEdge::Left;
        } else {
            // Clamp to prevent crossing
            if (_state.edge == DraggedEdge::Left) {
                _state.current_start = _state.current_end - _config.min_width;
            } else {
                _state.current_end = _state.current_start + _config.min_width;
            }
        }
    }
    
    // Enforce minimum width
    int64_t width = _state.current_end - _state.current_start;
    if (width < _config.min_width) {
        if (_state.edge == DraggedEdge::Left) {
            _state.current_start = _state.current_end - _config.min_width;
        } else {
            _state.current_end = _state.current_start + _config.min_width;
        }
    }
    
    // Enforce maximum width (if set)
    if (_config.max_width > 0 && width > _config.max_width) {
        if (_state.edge == DraggedEdge::Left) {
            _state.current_start = _state.current_end - _config.max_width;
        } else {
            _state.current_end = _state.current_start + _config.max_width;
        }
    }
    
    // Enforce time bounds (if set)
    if (_config.max_time > 0) {
        if (_state.current_end > _config.max_time) {
            _state.current_end = _config.max_time;
            // Re-check minimum width after boundary clamp
            if (_state.current_end - _state.current_start < _config.min_width) {
                _state.current_start = _state.current_end - _config.min_width;
            }
        }
        if (_state.current_start < _config.min_time) {
            _state.current_start = _config.min_time;
            // Re-check minimum width after boundary clamp
            if (_state.current_end - _state.current_start < _config.min_width) {
                _state.current_end = _state.current_start + _config.min_width;
            }
        }
    }
}

inline void IntervalDragController::reset() {
    _state = IntervalDragState{};
}

} // namespace CorePlotting

#endif // COREPLOTTING_INTERACTION_INTERVALDRAGCONTROLLER_HPP
