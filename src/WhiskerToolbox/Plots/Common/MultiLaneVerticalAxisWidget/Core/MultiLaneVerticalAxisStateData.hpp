/**
 * @file MultiLaneVerticalAxisStateData.hpp
 * @brief Data structures for the multi-lane vertical axis widget
 *
 * Provides serializable descriptors for a multi-lane Y-axis where each lane
 * has independent amplitude scaling. Used by DataViewer to display channel
 * names and lane separators alongside the OpenGL rendering surface.
 */

#ifndef MULTI_LANE_VERTICAL_AXIS_STATE_DATA_HPP
#define MULTI_LANE_VERTICAL_AXIS_STATE_DATA_HPP

#include <string>
#include <vector>

/**
 * @brief Descriptor for a single lane in the multi-lane vertical axis
 *
 * Each lane corresponds to one series in the layout. The axis widget uses
 * these descriptors to render channel labels and lane separators.
 */
struct LaneAxisDescriptor {
    std::string label;   ///< Channel/series display name
    float y_center{0.0f};///< Center Y position in NDC (from LayoutTransform offset)
    float y_extent{1.0f};///< Total height of the lane in NDC (from LayoutTransform gain * 2)
    int lane_index{0};   ///< Index in the stacking order
};

/**
 * @brief Serializable state data for the multi-lane vertical axis
 */
struct MultiLaneVerticalAxisStateData {
    std::vector<LaneAxisDescriptor> lanes;///< Descriptors for all lanes
    bool show_labels{false};              ///< Whether to show lane labels (tooltip-on-hover preferred)
    bool show_separators{true};           ///< Whether to show lane boundary lines
};

#endif// MULTI_LANE_VERTICAL_AXIS_STATE_DATA_HPP
