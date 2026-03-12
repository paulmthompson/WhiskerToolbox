/**
 * @file DataSynthesizerStateData.hpp
 * @brief Serializable state data structure for DataSynthesizer widget
 *
 * Separated from DataSynthesizerState.hpp so it can be included without
 * pulling in Qt dependencies.
 *
 * @see DataSynthesizerState for the Qt wrapper class
 */

#ifndef DATA_SYNTHESIZER_STATE_DATA_HPP
#define DATA_SYNTHESIZER_STATE_DATA_HPP

#include <rfl.hpp>

#include <string>

/**
 * @brief Serializable data structure for DataSynthesizerState
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataSynthesizerStateData {
    std::string instance_id;                      ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Synthesizer";///< User-visible name
};

#endif// DATA_SYNTHESIZER_STATE_DATA_HPP
