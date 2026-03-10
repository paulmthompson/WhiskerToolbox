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
    auto points = registry.getSupportedSaveFormats(IODataType::Points);
    REQUIRE(points.empty());

    auto analog = registry.getSupportedSaveFormats(IODataType::Analog);
    REQUIRE(analog.empty());
}

// ============================================================================
// Registry with loaders registered (loaders have no savers yet → still empty)
// ============================================================================

TEST_CASE("getSupportedSaveFormats returns empty when loaders have no savers", "[SaverRegistry]") {
    LoaderRegistry registry;

    // Register internal loaders — none override getSaverInfo() yet
    registerInternalLoaders(registry);

    auto all = registry.getSupportedSaveFormats();
    REQUIRE(all.empty());
}

TEST_CASE("getSupportedSaveFormats filtered by type returns empty when no savers registered", "[SaverRegistry]") {
    LoaderRegistry registry;
    registerInternalLoaders(registry);

    auto line_savers = registry.getSupportedSaveFormats(IODataType::Line);
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
                    IODataType /*dataType*/,
                    nlohmann::json const & /*config*/) const override {
        return LoadResult("stub");
    }

    bool supportsFormat(std::string const & format,
                        IODataType dataType) const override {
        return format == "stub" && dataType == IODataType::Points;
    }

    std::string getLoaderName() const override { return "StubLoaderWithSaver"; }

    std::vector<SaverInfo> getSaverInfo() const override {
        SaverInfo info;
        info.format = "stub";
        info.data_type = IODataType::Points;
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
    REQUIRE(all[0].data_type == IODataType::Points);
    REQUIRE(all[0].description == "Stub point saver for testing");
}

TEST_CASE("getSupportedSaveFormats filters by data type correctly", "[SaverRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderWithSaver>());

    auto points = registry.getSupportedSaveFormats(IODataType::Points);
    REQUIRE(points.size() == 1);
    REQUIRE(points[0].format == "stub");

    auto lines = registry.getSupportedSaveFormats(IODataType::Line);
    REQUIRE(lines.empty());

    auto analog = registry.getSupportedSaveFormats(IODataType::Analog);
    REQUIRE(analog.empty());
}
