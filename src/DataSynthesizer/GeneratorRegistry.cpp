/**
 * @file GeneratorRegistry.cpp
 * @brief Implementation of the GeneratorRegistry singleton.
 */
#include "GeneratorRegistry.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace WhiskerToolbox::DataSynthesizer {

GeneratorRegistry & GeneratorRegistry::instance() {
    static GeneratorRegistry registry;
    return registry;
}

void GeneratorRegistry::registerGenerator(
        std::string const & name,
        GeneratorFunction func,
        GeneratorMetadata metadata) {
    assert(!name.empty() && "Generator name must not be empty");
    assert(func && "Generator function must not be null");

    if (entries_.contains(name)) {
        std::cerr << "DataSynthesizer: Generator '" << name
                  << "' is already registered. Skipping duplicate.\n";
        return;
    }

    metadata.name = name;
    auto const & output_type = metadata.output_type;

    output_type_index_[output_type].push_back(name);
    entries_[name] = GeneratorEntry{std::move(func), std::move(metadata)};
}

std::optional<DataTypeVariant> GeneratorRegistry::generate(
        std::string const & name,
        std::string const & params_json,
        GeneratorContext const & ctx) const {
    auto const it = entries_.find(name);
    if (it == entries_.end()) {
        std::cerr << "DataSynthesizer: Unknown generator '" << name << "'\n";
        return std::nullopt;
    }

    try {
        return it->second.function(params_json, ctx);
    } catch (std::exception const & e) {
        std::cerr << "DataSynthesizer: Generator '" << name
                  << "' failed: " << e.what() << "\n";
        return std::nullopt;
    }
}

std::vector<std::string> GeneratorRegistry::listGenerators(
        std::string const & output_type) const {
    auto const it = output_type_index_.find(output_type);
    if (it == output_type_index_.end()) {
        return {};
    }
    return it->second;
}

std::vector<std::string> GeneratorRegistry::listAllGenerators() const {
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (auto const & [name, _]: entries_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> GeneratorRegistry::listOutputTypes() const {
    std::vector<std::string> types;
    types.reserve(output_type_index_.size());
    for (auto const & [type, _]: output_type_index_) {
        types.push_back(type);
    }
    std::sort(types.begin(), types.end());
    return types;
}

std::optional<ParameterSchema> GeneratorRegistry::getSchema(
        std::string const & name) const {
    auto const it = entries_.find(name);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second.metadata.parameter_schema;
}

std::optional<GeneratorMetadata> GeneratorRegistry::getMetadata(
        std::string const & name) const {
    auto const it = entries_.find(name);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second.metadata;
}

bool GeneratorRegistry::hasGenerator(std::string const & name) const {
    return entries_.contains(name);
}

std::size_t GeneratorRegistry::size() const {
    return entries_.size();
}

}// namespace WhiskerToolbox::DataSynthesizer
