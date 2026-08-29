/**
 * @file ProbeGeometry.test.cpp
 * @brief Unit tests for CoreUtilities probe geometry structures and SpikeSorter .cfg parser.
 */

#include "CoreUtilities/ProbeGeometry/ChannelPosition.hpp"
#include "CoreUtilities/ProbeGeometry/SwindaleSpikeSorterLoader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>
#include <vector>

namespace {

char const * kPoly3ElectrodeCfg =
        "poly3\n"
        "1 1 0 275\n"
        "2 2 18 212.5\n"
        "3 3 0 175\n"
        "7 7 18, 187.5\n"
        "20 20 0 0\n";

}// namespace

TEST_CASE("CoreUtilities parseSwindaleSpikeSorterConfig parses poly3 probe geometry", "[CoreUtilities][ProbeGeometry]") {
    auto const positions = parseSwindaleSpikeSorterConfig(kPoly3ElectrodeCfg);
    REQUIRE(positions.size() == 5);

    // Channel 1 -> 0-based index 0, x=0, y=275
    REQUIRE(positions[0].channel_id == 0);
    REQUIRE_THAT(positions[0].x, Catch::Matchers::WithinRel(0.0f, 1e-4f));
    REQUIRE_THAT(positions[0].y, Catch::Matchers::WithinRel(275.0f, 1e-4f));

    // Channel 2 -> 0-based index 1, x=18, y=212.5
    REQUIRE(positions[1].channel_id == 1);
    REQUIRE_THAT(positions[1].x, Catch::Matchers::WithinRel(18.0f, 1e-4f));
    REQUIRE_THAT(positions[1].y, Catch::Matchers::WithinRel(212.5f, 1e-4f));

    // Channel 7 -> 0-based index 6, x=18, y=187.5 (with comma separator)
    REQUIRE(positions[3].channel_id == 6);
    REQUIRE_THAT(positions[3].x, Catch::Matchers::WithinRel(18.0f, 1e-4f));
    REQUIRE_THAT(positions[3].y, Catch::Matchers::WithinRel(187.5f, 1e-4f));

    // Channel 20 -> 0-based index 19, x=0, y=0
    REQUIRE(positions[4].channel_id == 19);
    REQUIRE_THAT(positions[4].x, Catch::Matchers::WithinRel(0.0f, 1e-4f));
    REQUIRE_THAT(positions[4].y, Catch::Matchers::WithinRel(0.0f, 1e-4f));
}

TEST_CASE("CoreUtilities series identity parsing and channel extraction", "[CoreUtilities][ProbeGeometry]") {
    SECTION("parseSeriesIdentity with valid suffixes") {
        auto const id1 = parseSeriesIdentity("ephys_32");
        REQUIRE(id1.group == "ephys");
        REQUIRE(id1.channel_id.has_value());
        REQUIRE(*id1.channel_id == 31);// 1-based suffix 32 -> 0-based channel 31

        auto const id2 = parseSeriesIdentity("voltage_raw_1");
        REQUIRE(id2.group == "voltage_raw");
        REQUIRE(id2.channel_id.has_value());
        REQUIRE(*id2.channel_id == 0);// 1-based suffix 1 -> 0-based channel 0

        auto const id_zero = parseSeriesIdentity("voltage_0");
        REQUIRE(id_zero.group == "voltage");
        REQUIRE_FALSE(id_zero.channel_id.has_value());// 0-based suffix returns nullopt

        auto const id3 = parseSeriesIdentity("stimulus");
        REQUIRE(id3.group == "stimulus");
        REQUIRE_FALSE(id3.channel_id.has_value());
    }

    SECTION("extractChannelFromSeriesKey with 1-based and 0-based indexing") {
        auto const ch_one = extractChannelFromSeriesKey("ephys_1", "ephys", true);
        REQUIRE(ch_one.has_value());
        REQUIRE(*ch_one == 0);

        auto const ch_zero = extractChannelFromSeriesKey("ephys_0", "ephys", false);
        REQUIRE(ch_zero.has_value());
        REQUIRE(*ch_zero == 0);
    }
}
