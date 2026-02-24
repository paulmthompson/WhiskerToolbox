#ifndef TIMESCROLLBAR_STATE_DATA_HPP
#define TIMESCROLLBAR_STATE_DATA_HPP

/**
 * @file TimeScrollBarStateData.hpp
 * @brief Serializable state data structure for TimeScrollBar
 *
 * Separated from TimeScrollBarState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see TimeScrollBarState for the Qt wrapper class
 */

#include <rfl.hpp>

#include <string>

/**
 * @brief Serializable data structure for TimeScrollBarState
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct TimeScrollBarStateData {
    std::string instance_id;                          ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Timeline";            ///< User-visible name

    // Playback parameters
    int play_speed = 1;                               ///< Play speed multiplier (1x, 2x, etc.)
    int frame_jump = 10;                              ///< Frame jump value for keyboard shortcuts
    bool is_playing = false;                          ///< Whether video is currently playing
};

#endif // TIMESCROLLBAR_STATE_DATA_HPP
