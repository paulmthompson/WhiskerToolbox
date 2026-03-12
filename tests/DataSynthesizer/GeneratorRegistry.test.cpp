/**
 * @file GeneratorRegistry.test.cpp
 * @brief Tests for the DataSynthesizer GeneratorRegistry and its integration
 *        with the five initial AnalogTimeSeries generators.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace WhiskerToolbox::DataSynthesizer;

TEST_CASE("GeneratorRegistry lists all five analog generators", "[registry]") {
    auto const names = GeneratorRegistry::instance().listGenerators("AnalogTimeSeries");
    REQUIRE(names.size() >= 5);

    auto contains = [&](std::string const & name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };
    REQUIRE(contains("SineWave"));
    REQUIRE(contains("SquareWave"));
    REQUIRE(contains("TriangleWave"));
    REQUIRE(contains("GaussianNoise"));
    REQUIRE(contains("UniformNoise"));
}

TEST_CASE("GeneratorRegistry hasGenerator returns true for registered generators", "[registry]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE(reg.hasGenerator("SineWave"));
    REQUIRE(reg.hasGenerator("SquareWave"));
    REQUIRE(reg.hasGenerator("TriangleWave"));
    REQUIRE(reg.hasGenerator("GaussianNoise"));
    REQUIRE(reg.hasGenerator("UniformNoise"));
}

TEST_CASE("GeneratorRegistry hasGenerator returns false for unknown generator", "[registry]") {
    REQUIRE_FALSE(GeneratorRegistry::instance().hasGenerator("NonExistentGenerator_XYZ"));
}

TEST_CASE("GeneratorRegistry generate returns nullopt for unknown generator", "[registry]") {
    auto result = GeneratorRegistry::instance().generate("NoSuchGenerator", "{}");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GeneratorRegistry getMetadata returns category for SineWave", "[registry]") {
    auto meta = GeneratorRegistry::instance().getMetadata("SineWave");
    REQUIRE(meta.has_value());
    REQUIRE(meta->category == "Periodic");
    REQUIRE(meta->output_type == "AnalogTimeSeries");
}

TEST_CASE("GeneratorRegistry getSchema returns schema for SineWave", "[registry]") {
    auto schema = GeneratorRegistry::instance().getSchema("SineWave");
    REQUIRE(schema.has_value());
    REQUIRE_FALSE(schema->fields.empty());
}
