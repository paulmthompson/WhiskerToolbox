/**
 * @file GeneratorRegistry.hpp
 * @brief Singleton registry for data generators.
 *
 * Maps (output_type, generator_name) pairs to GeneratorEntry structs containing
 * the generator function and metadata. Generators self-register at static init
 * time using the Registration.hpp RAII helper.
 */
#ifndef WHISKERTOOLBOX_GENERATOR_REGISTRY_HPP
#define WHISKERTOOLBOX_GENERATOR_REGISTRY_HPP

#include "GeneratorTypes.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace WhiskerToolbox::DataSynthesizer {

/**
 * @brief Singleton registry for data generators.
 *
 * Provides registration, lookup, and execution of generators keyed by
 * (output_type, generator_name). Follows the same singleton pattern as
 * TransformsV2's ElementRegistry.
 */
class GeneratorRegistry {
public:
    GeneratorRegistry() = default;

    GeneratorRegistry(GeneratorRegistry const &) = delete;
    GeneratorRegistry & operator=(GeneratorRegistry const &) = delete;
    GeneratorRegistry(GeneratorRegistry &&) = default;
    GeneratorRegistry & operator=(GeneratorRegistry &&) = default;

    /**
     * @brief Get the global singleton instance.
     */
    static GeneratorRegistry & instance();

    /**
     * @brief Register a generator.
     *
     * @pre No generator with the same name must already be registered.
     *
     * @param name Unique generator name (e.g., "SineWave")
     * @param func The generator function
     * @param metadata Metadata describing the generator
     */
    void registerGenerator(
            std::string const & name,
            GeneratorFunction func,
            GeneratorMetadata metadata);

    /**
     * @brief Execute a generator by name.
     *
     * @param name The generator name
     * @param params_json JSON string of generator-specific parameters
     * @param ctx Optional context providing DataManager access
     * @return The generated data as a DataTypeVariant, or std::nullopt on failure
     */
    [[nodiscard]] std::optional<DataTypeVariant> generate(
            std::string const & name,
            std::string const & params_json,
            GeneratorContext const & ctx = {}) const;

    /**
     * @brief List all registered generator names for a given output type.
     *
     * @param output_type Data type string (e.g., "AnalogTimeSeries")
     * @return Vector of generator names producing that output type
     */
    [[nodiscard]] std::vector<std::string> listGenerators(std::string const & output_type) const;

    /**
     * @brief List all registered generator names (all output types).
     */
    [[nodiscard]] std::vector<std::string> listAllGenerators() const;

    /**
     * @brief List all unique output types that have at least one registered generator.
     */
    [[nodiscard]] std::vector<std::string> listOutputTypes() const;

    /**
     * @brief Get the parameter schema for a generator.
     *
     * @param name The generator name
     * @return The ParameterSchema, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<Transforms::V2::ParameterSchema> getSchema(
            std::string const & name) const;

    /**
     * @brief Get the metadata for a generator.
     *
     * @param name The generator name
     * @return The GeneratorMetadata, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<GeneratorMetadata> getMetadata(
            std::string const & name) const;

    /**
     * @brief Check if a generator is registered.
     */
    [[nodiscard]] bool hasGenerator(std::string const & name) const;

    /**
     * @brief Get the number of registered generators.
     */
    [[nodiscard]] std::size_t size() const;

private:
    /// Primary storage: name → entry
    std::unordered_map<std::string, GeneratorEntry> entries_;

    /// Index: output_type → list of generator names
    std::unordered_map<std::string, std::vector<std::string>> output_type_index_;
};

}// namespace WhiskerToolbox::DataSynthesizer

#endif// WHISKERTOOLBOX_GENERATOR_REGISTRY_HPP
