/**
 * @file Registration.hpp
 * @brief RAII helper for static registration of data generators.
 *
 * Provides RegisterGenerator, a template class that registers a generator
 * function into the GeneratorRegistry at static initialization time.
 * Each generator .cpp file creates a file-scope instance in an anonymous
 * namespace to self-register.
 *
 * Usage example:
 * @code
 * #include "DataSynthesizer/Registration.hpp"
 * #include "DataSynthesizer/GeneratorRegistry.hpp"
 *
 * namespace {
 * auto const reg = WhiskerToolbox::DataSynthesizer::RegisterGenerator<SineWaveParams>(
 *     "SineWave",
 *     generateSineWave,
 *     WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
 *         .description = "Generate a sine wave",
 *         .category = "Periodic",
 *         .output_type = "AnalogTimeSeries"
 *     });
 * }
 * @endcode
 */
#ifndef WHISKERTOOLBOX_DATASYNTHESIZER_REGISTRATION_HPP
#define WHISKERTOOLBOX_DATASYNTHESIZER_REGISTRATION_HPP

#include "GeneratorRegistry.hpp"
#include "GeneratorTypes.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl/json.hpp>

#include <iostream>
#include <string>

namespace WhiskerToolbox::DataSynthesizer {

/**
 * @brief RAII helper for compile-time generator registration.
 *
 * @tparam Params The generator's parameter struct (must be reflect-cpp reflectable).
 *
 * The constructor:
 * 1. Wraps the typed generate function to accept a JSON string.
 * 2. Extracts the ParameterSchema from Params.
 * 3. Registers the generator into the global GeneratorRegistry.
 *
 * The typed generate function has the signature:
 *   DataTypeVariant(Params const &)
 *
 * The wrapper handles JSON deserialization, so individual generators
 * work with strongly-typed parameter structs.
 */
template<typename Params>
class RegisterGenerator {
public:
    RegisterGenerator(
            std::string const & name,
            std::function<DataTypeVariant(Params const &)> typed_func,
            GeneratorMetadata metadata) {

        // Wrap the typed function to accept a JSON string
        auto wrapper = [typed_func = std::move(typed_func), name](
                               std::string const & params_json) -> DataTypeVariant {
            auto parsed = rfl::json::read<Params>(params_json);
            if (!parsed) {
                throw std::runtime_error(
                        "Generator '" + name + "': failed to parse parameters");
            }
            return typed_func(*parsed);
        };

        // Extract parameter schema from the Params type
        metadata.parameter_schema = Transforms::V2::extractParameterSchema<Params>();

        GeneratorRegistry::instance().registerGenerator(
                name,
                std::move(wrapper),
                std::move(metadata));
    }
};

}// namespace WhiskerToolbox::DataSynthesizer

#endif// WHISKERTOOLBOX_DATASYNTHESIZER_REGISTRATION_HPP
