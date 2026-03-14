/**
 * @file saver_registry.test.cpp
 * @brief Tests for SaverInfo and getSupportedSaveFormats() registry queries.
 */
#include <catch2/catch_test_macros.hpp>

#include "IO/core/LoaderRegistration.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/core/SaverInfo.hpp"

#include <string>
#include <vector>

// ============================================================================
// Fresh registry tests (no savers registered)
// ============================================================================

TEST_CASE("getSupportedSaveFormats returns empty for fresh registry", "[SaverRegistry]") {
    LoaderRegistry registry;
    auto all = registry.getSupportedSaveFormats();
    REQUIRE(all.empty());
}

TEST_CASE("getSupportedSaveFormats with dataType returns empty for fresh registry", "[SaverRegistry]") {
    LoaderRegistry registry;
    auto points = registry.getSupportedSaveFormats(DM_DataType::Points);
    REQUIRE(points.empty());

    auto analog = registry.getSupportedSaveFormats(DM_DataType::Analog);
    REQUIRE(analog.empty());
}

// ============================================================================
// Registry with loaders that have no savers → queries return empty
// ============================================================================

namespace {

/// Minimal stub loader that does NOT override getSaverInfo() (uses default empty).
class StubLoaderNoSaver : public IFormatLoader {
public:
    LoadResult load(std::string const & /*filepath*/,
                    DM_DataType /*dataType*/,
                    nlohmann::json const & /*config*/) const override {
        return LoadResult("stub-no-saver");
    }

    bool supportsFormat(std::string const & format,
                        DM_DataType dataType) const override {
        return format == "nosaver" && dataType == DM_DataType::Line;
    }

    std::string getLoaderName() const override { return "StubLoaderNoSaver"; }
};

}// anonymous namespace

TEST_CASE("getSupportedSaveFormats returns empty when loaders have no savers", "[SaverRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderNoSaver>());

    auto all = registry.getSupportedSaveFormats();
    REQUIRE(all.empty());
}

TEST_CASE("getSupportedSaveFormats filtered by type returns empty when no savers registered", "[SaverRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderNoSaver>());

    auto line_savers = registry.getSupportedSaveFormats(DM_DataType::Line);
    REQUIRE(line_savers.empty());
}

// ============================================================================
// Custom loader with savers (verifies plumbing)
// ============================================================================

namespace {

/// Minimal stub loader that reports one saver via getSaverInfo().
class StubLoaderWithSaver : public IFormatLoader {
public:
    LoadResult load(std::string const & /*filepath*/,
                    DM_DataType /*dataType*/,
                    nlohmann::json const & /*config*/) const override {
        return LoadResult("stub");
    }

    bool supportsFormat(std::string const & format,
                        DM_DataType dataType) const override {
        return format == "stub" && dataType == DM_DataType::Points;
    }

    std::string getLoaderName() const override { return "StubLoaderWithSaver"; }

    std::vector<SaverInfo> getSaverInfo() const override {
        SaverInfo info;
        info.format = "stub";
        info.data_type = DM_DataType::Points;
        info.description = "Stub point saver for testing";
        // schema left default-constructed (empty)
        return {info};
    }
};

}// anonymous namespace

TEST_CASE("getSupportedSaveFormats returns entries from registered loaders", "[SaverRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderWithSaver>());

    auto all = registry.getSupportedSaveFormats();
    REQUIRE(all.size() == 1);
    REQUIRE(all[0].format == "stub");
    REQUIRE(all[0].data_type == DM_DataType::Points);
    REQUIRE(all[0].description == "Stub point saver for testing");
}

TEST_CASE("getSupportedSaveFormats filters by data type correctly", "[SaverRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderWithSaver>());

    auto points = registry.getSupportedSaveFormats(DM_DataType::Points);
    REQUIRE(points.size() == 1);
    REQUIRE(points[0].format == "stub");

    auto lines = registry.getSupportedSaveFormats(DM_DataType::Line);
    REQUIRE(lines.empty());

    auto analog = registry.getSupportedSaveFormats(DM_DataType::Analog);
    REQUIRE(analog.empty());
}

// ============================================================================
// Real internal loaders return CSV savers
// ============================================================================

TEST_CASE("registerInternalLoaders exposes CSV savers", "[SaverRegistry]") {
    LoaderRegistry registry;
    registerInternalLoaders(registry);

    auto all = registry.getSupportedSaveFormats();
    REQUIRE_FALSE(all.empty());

    auto point_savers = registry.getSupportedSaveFormats(DM_DataType::Points);
    REQUIRE_FALSE(point_savers.empty());
    REQUIRE(point_savers[0].format == "csv");

    auto analog_savers = registry.getSupportedSaveFormats(DM_DataType::Analog);
    REQUIRE_FALSE(analog_savers.empty());
}
