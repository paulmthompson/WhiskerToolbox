#ifndef PLOT_VIEW_AXIS_REFRESH_HPP
#define PLOT_VIEW_AXIS_REFRESH_HPP

/**
 * @file PlotViewAxisRefresh.hpp
 * @brief Axis-selective refresh helpers for plot widget view updates
 *
 * Used by plot container widgets to repaint and sync only the axis widgets
 * affected by a view-state change (e.g. X-only wheel zoom skips vertical axis).
 */

#include <concepts>
#include <cstdint>
#include <optional>
#include <utility>

namespace Neuralyzer::Plots {

/**
 * @brief Which plot axes need refresh after a view-state change.
 */
enum class ViewAxisRefresh : std::uint8_t {
    None = 0,
    Horizontal = 1 << 0,
    Vertical = 1 << 1,
    Both = Horizontal | Vertical,
};

/**
 * @brief Test whether @p mask includes @p axis.
 */
[[nodiscard]] constexpr bool affectsAxis(ViewAxisRefresh mask, ViewAxisRefresh axis) {
    return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(axis)) != 0U;
}

/**
 * @brief Snapshot of view fields that affect axis display.
 */
struct ViewAxisSyncSnapshot {
    double x_zoom{1.0};
    double y_zoom{1.0};
    double x_pan{0.0};
    double y_pan{0.0};
    std::optional<std::pair<double, double>> x_bounds;
    std::optional<std::pair<double, double>> y_bounds;
};

template<typename T>
concept ViewStateZoomPanLike = requires(T const & vs) {
    { vs.x_zoom } -> std::convertible_to<double>;
    { vs.y_zoom } -> std::convertible_to<double>;
    { vs.x_pan } -> std::convertible_to<double>;
    { vs.y_pan } -> std::convertible_to<double>;
};

template<typename T>
concept ViewStateBoundsLike = ViewStateZoomPanLike<T> && requires(T const & vs) {
    { vs.x_min } -> std::convertible_to<double>;
    { vs.x_max } -> std::convertible_to<double>;
    { vs.y_min } -> std::convertible_to<double>;
    { vs.y_max } -> std::convertible_to<double>;
};

/**
 * @brief Capture zoom/pan fields from a view state.
 */
template<ViewStateZoomPanLike ViewState>
[[nodiscard]] ViewAxisSyncSnapshot makeViewAxisSyncSnapshot(ViewState const & view_state) {
    return ViewAxisSyncSnapshot{
            .x_zoom = static_cast<double>(view_state.x_zoom),
            .y_zoom = static_cast<double>(view_state.y_zoom),
            .x_pan = static_cast<double>(view_state.x_pan),
            .y_pan = static_cast<double>(view_state.y_pan),
            .x_bounds = std::nullopt,
            .y_bounds = std::nullopt,
    };
}

/**
 * @brief Capture zoom/pan and data bounds from a view state.
 */
template<ViewStateBoundsLike ViewState>
[[nodiscard]] ViewAxisSyncSnapshot makeViewAxisSyncSnapshot(ViewState const & view_state) {
    return ViewAxisSyncSnapshot{
            .x_zoom = static_cast<double>(view_state.x_zoom),
            .y_zoom = static_cast<double>(view_state.y_zoom),
            .x_pan = static_cast<double>(view_state.x_pan),
            .y_pan = static_cast<double>(view_state.y_pan),
            .x_bounds = std::make_pair(
                    static_cast<double>(view_state.x_min),
                    static_cast<double>(view_state.x_max)),
            .y_bounds = std::make_pair(
                    static_cast<double>(view_state.y_min),
                    static_cast<double>(view_state.y_max)),
    };
}

/**
 * @brief Determine which axes need refresh between two snapshots.
 */
[[nodiscard]] inline ViewAxisRefresh computeViewAxisRefreshMask(
        ViewAxisSyncSnapshot const & before,
        ViewAxisSyncSnapshot const & after) {
    ViewAxisRefresh mask = ViewAxisRefresh::None;

    bool const horizontal_changed =
            before.x_zoom != after.x_zoom || before.x_pan != after.x_pan ||
            (before.x_bounds != after.x_bounds);
    bool const vertical_changed =
            before.y_zoom != after.y_zoom || before.y_pan != after.y_pan ||
            (before.y_bounds != after.y_bounds);

    if (horizontal_changed) {
        mask = static_cast<ViewAxisRefresh>(
                static_cast<std::uint8_t>(mask) |
                static_cast<std::uint8_t>(ViewAxisRefresh::Horizontal));
    }
    if (vertical_changed) {
        mask = static_cast<ViewAxisRefresh>(
                static_cast<std::uint8_t>(mask) |
                static_cast<std::uint8_t>(ViewAxisRefresh::Vertical));
    }

    return mask;
}

}// namespace Neuralyzer::Plots

#endif// PLOT_VIEW_AXIS_REFRESH_HPP
