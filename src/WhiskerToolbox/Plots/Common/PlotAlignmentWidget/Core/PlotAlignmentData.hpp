#ifndef PLOT_ALIGNMENT_DATA_HPP
#define PLOT_ALIGNMENT_DATA_HPP

/**
 * @file PlotAlignmentData.hpp
 * @brief Data structures for plot alignment functionality
 * 
 * This header defines the data structures used for event/interval alignment
 * in plot widgets. These structures can be included in plot state data structs
 * for serialization.
 */

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Enumeration for interval alignment type
 */
enum class IntervalAlignmentType {
    Beginning,
    End
};

/**
 * @brief Serializable data for plot alignment settings
 * 
 * This struct contains all alignment-related settings that can be
 * included in plot state data structs for serialization.
 */
struct PlotAlignmentData {
    std::string alignment_event_key;                                                 ///< Key of the selected event/interval series for alignment
    IntervalAlignmentType interval_alignment_type = IntervalAlignmentType::Beginning;///< For intervals: use beginning or end
    double offset = 0.0;                                                             ///< Offset in time units to apply to alignment events
    double window_size = 1000.0;                                                     ///< Window size in time units to gather around alignment event
};

#endif// PLOT_ALIGNMENT_DATA_HPP
