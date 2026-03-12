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

    // Generator selection
    std::string output_type;   ///< e.g. "AnalogTimeSeries", "DigitalEventSeries"
    std::string generator_name;///< Registry key (e.g. "SineWave")
    std::string parameter_json;///< Current params as JSON string

    // Output configuration
    std::string output_key;       ///< DataManager key for generated data
    std::string time_key = "time";///< TimeFrame association
};

#endif// DATA_SYNTHESIZER_STATE_DATA_HPP
