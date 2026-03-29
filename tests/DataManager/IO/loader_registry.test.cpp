/**
 * @file loader_registry.test.cpp
 * @brief Tests for LoaderInfo and getSupportedLoadFormats() registry queries.
 */
#include <catch2/catch_test_macros.hpp>

#include "IO/core/LoaderInfo.hpp"
#include "IO/core/LoaderRegistration.hpp"
#include "IO/core/LoaderRegistry.hpp"

#include <string>
#include <vector>

// ============================================================================
// Fresh registry tests (no loaders registered)
// ============================================================================

TEST_CASE("getSupportedLoadFormats returns empty for fresh registry", "[LoaderRegistry]") {
    LoaderRegistry const registry;
    auto all = registry.getSupportedLoadFormats();
    REQUIRE(all.empty());
}

TEST_CASE("getSupportedLoadFormats with dataType returns empty for fresh registry", "[LoaderRegistry]") {
    LoaderRegistry const registry;
    auto points = registry.getSupportedLoadFormats(DM_DataType::Points);
    REQUIRE(points.empty());

    auto analog = registry.getSupportedLoadFormats(DM_DataType::Analog);
    REQUIRE(analog.empty());
}

// ============================================================================
// Registry with loaders that have no LoaderInfo → queries return empty
// ============================================================================

namespace {

/// Minimal stub loader that does NOT override getLoaderInfo() (uses default empty).
class StubLoaderNoInfo : public IFormatLoader {
public:
    [[nodiscard]] LoadResult load(std::string const & /*filepath*/,
                                  DM_DataType /*dataType*/,
                                  nlohmann::json const & /*config*/) const override {
        return LoadResult("stub-no-info");
    }

    [[nodiscard]] bool supportsFormat(std::string const & format,
                                      DM_DataType dataType) const override {
        return format == "noinfo" && dataType == DM_DataType::Line;
    }

    [[nodiscard]] std::string getLoaderName() const override { return "StubLoaderNoInfo"; }
};

}// anonymous namespace

TEST_CASE("getSupportedLoadFormats returns empty when loaders have no info", "[LoaderRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderNoInfo>());

    auto all = registry.getSupportedLoadFormats();
    REQUIRE(all.empty());
}

TEST_CASE("getSupportedLoadFormats filtered by type returns empty when no info registered", "[LoaderRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderNoInfo>());

    auto line_loaders = registry.getSupportedLoadFormats(DM_DataType::Line);
    REQUIRE(line_loaders.empty());
}

// ============================================================================
// Custom loader with LoaderInfo (verifies plumbing)
// ============================================================================

namespace {

/// Minimal stub loader that reports one loader via getLoaderInfo().
class StubLoaderWithInfo : public IFormatLoader {
public:
    [[nodiscard]] LoadResult load(std::string const & /*filepath*/,
                                  DM_DataType /*dataType*/,
                                  nlohmann::json const & /*config*/) const override {
        return LoadResult("stub");
    }

    [[nodiscard]] bool supportsFormat(std::string const & format,
                                      DM_DataType dataType) const override {
        return format == "stub" && dataType == DM_DataType::Points;
    }

    [[nodiscard]] std::string getLoaderName() const override { return "StubLoaderWithInfo"; }

    [[nodiscard]] std::vector<LoaderInfo> getLoaderInfo() const override {
        LoaderInfo info;
        info.format = "stub";
        info.data_type = DM_DataType::Points;
        info.description = "Stub point loader for testing";
        info.supports_batch = false;
        // schema left default-constructed (empty)
        return {info};
    }
};

/// Stub loader with batch support flag set.
class StubBatchLoader : public IFormatLoader {
public:
    [[nodiscard]] LoadResult load(std::string const & /*filepath*/,
                                  DM_DataType /*dataType*/,
                                  nlohmann::json const & /*config*/) const override {
        return LoadResult("stub-batch");
    }

    [[nodiscard]] bool supportsFormat(std::string const & format,
                                      DM_DataType dataType) const override {
        return format == "stub_batch" && dataType == DM_DataType::Analog;
    }

    [[nodiscard]] std::string getLoaderName() const override { return "StubBatchLoader"; }

    [[nodiscard]] std::vector<LoaderInfo> getLoaderInfo() const override {
        LoaderInfo info;
        info.format = "stub_batch";
        info.data_type = DM_DataType::Analog;
        info.description = "Stub batch analog loader for testing";
        info.supports_batch = true;
        return {info};
    }
};

}// anonymous namespace

TEST_CASE("getSupportedLoadFormats returns entries from registered loaders", "[LoaderRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderWithInfo>());

    auto all = registry.getSupportedLoadFormats();
    REQUIRE(all.size() == 1);
    REQUIRE(all[0].format == "stub");
    REQUIRE(all[0].data_type == DM_DataType::Points);
    REQUIRE(all[0].description == "Stub point loader for testing");
    REQUIRE_FALSE(all[0].supports_batch);
}

TEST_CASE("getSupportedLoadFormats filters by data type correctly", "[LoaderRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderWithInfo>());

    auto points = registry.getSupportedLoadFormats(DM_DataType::Points);
    REQUIRE(points.size() == 1);
    REQUIRE(points[0].format == "stub");

    auto lines = registry.getSupportedLoadFormats(DM_DataType::Line);
    REQUIRE(lines.empty());

    auto analog = registry.getSupportedLoadFormats(DM_DataType::Analog);
    REQUIRE(analog.empty());
}

TEST_CASE("getSupportedLoadFormats aggregates from multiple loaders", "[LoaderRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubLoaderWithInfo>());
    registry.registerLoader(std::make_unique<StubBatchLoader>());

    auto all = registry.getSupportedLoadFormats();
    REQUIRE(all.size() == 2);

    auto points = registry.getSupportedLoadFormats(DM_DataType::Points);
    REQUIRE(points.size() == 1);

    auto analog = registry.getSupportedLoadFormats(DM_DataType::Analog);
    REQUIRE(analog.size() == 1);
    REQUIRE(analog[0].supports_batch);
}

TEST_CASE("LoaderInfo supports_batch flag is preserved through registry", "[LoaderRegistry]") {
    LoaderRegistry registry;
    registry.registerLoader(std::make_unique<StubBatchLoader>());

    auto all = registry.getSupportedLoadFormats();
    REQUIRE(all.size() == 1);
    REQUIRE(all[0].supports_batch);
}
