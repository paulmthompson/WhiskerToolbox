/**
 * @file PostEncoderModuleRegistry.test.cpp
 * @brief Tests for PostEncoderModuleRegistry registration and lookup.
 */

#include <catch2/catch_test_macros.hpp>

#include "post_encoder/PostEncoderModuleRegistry.hpp"

TEST_CASE("PostEncoderModuleRegistry lists built-in modules",
          "[post_encoder][PostEncoderModuleRegistry]") {
    auto & registry = dl::PostEncoderModuleRegistry::instance();

    REQUIRE(registry.hasModule("global_avg_pool"));
    REQUIRE(registry.hasModule("spatial_point"));

    auto const keys = registry.moduleKeys();
    REQUIRE(keys.size() >= 2);
}

TEST_CASE("PostEncoderModuleRegistry getSchema returns fields for spatial_point",
          "[post_encoder][PostEncoderModuleRegistry]") {
    auto const schema =
            dl::PostEncoderModuleRegistry::instance().getSchema("spatial_point");
    REQUIRE(schema);
    CHECK(schema->field("interpolation") != nullptr);
    auto const * point_key = schema->field("point_key");
    REQUIRE(point_key != nullptr);
    CHECK(point_key->dynamic_combo);
    CHECK(point_key->include_none_sentinel);
}

TEST_CASE("PostEncoderModuleRegistry outputShape propagates through module",
          "[post_encoder][PostEncoderModuleRegistry]") {
    auto const shape = dl::PostEncoderModuleRegistry::instance().outputShape(
            "global_avg_pool",
            {384, 7, 7},
            "{}",
            {256, 256});
    REQUIRE(shape);
    REQUIRE(shape->size() == 1);
    CHECK((*shape)[0] == 384);
}
