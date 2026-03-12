/**
 * @file GeneratorTypes.hpp
 * @brief Type definitions for the DataSynthesizer generator system.
 *
 * Defines GeneratorMetadata, the generator function signature, and
 * GeneratorEntry used by the GeneratorRegistry. These types mirror the
 * TransformsV2 pattern but are tailored for data generators that produce
 * data from parameters alone (no input data required).
 */
#ifndef WHISKERTOOLBOX_GENERATOR_TYPES_HPP
#define WHISKERTOOLBOX_GENERATOR_TYPES_HPP

#include "DataManager/DataManagerTypes.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <functional>
#include <string>

namespace WhiskerToolbox::DataSynthesizer {

/**
 * @brief Metadata describing a registered generator
 *
 * Stored alongside each generator function in the registry. Provides
 * information for UI display, categorization, and parameter schema.
 */
struct GeneratorMetadata {
    std::string name;       ///< Unique generator name (e.g., "SineWave")
    std::string description;///< Human-readable description
    std::string category;   ///< Grouping category (e.g., "Periodic", "Noise")
    std::string output_type;///< DataManager type string (e.g., "AnalogTimeSeries")

    Transforms::V2::ParameterSchema parameter_schema;///< Schema for AutoParamWidget
};

/**
 * @brief Type alias for the generator function signature.
 *
 * A generator takes a JSON string of parameters and returns a DataTypeVariant.
 * The generator is responsible for deserializing its own parameter struct from
 * the JSON string using rfl::json::read<SpecificParams>.
 */
using GeneratorFunction = std::function<DataTypeVariant(std::string const & params_json)>;

/**
 * @brief A registry entry pairing a generator function with its metadata.
 */
struct GeneratorEntry {
    GeneratorFunction function;
    GeneratorMetadata metadata;
};

}// namespace WhiskerToolbox::DataSynthesizer

#endif// WHISKERTOOLBOX_GENERATOR_TYPES_HPP
