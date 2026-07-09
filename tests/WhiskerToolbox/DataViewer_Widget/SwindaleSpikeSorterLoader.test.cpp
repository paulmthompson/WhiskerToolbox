/**
 * @file SwindaleSpikeSorterLoader.test.cpp
 * @brief Unit tests for SpikeSorter electrode config parsing and Y-based rank ordering.
 */

#include "Ordering/SwindaleSpikeSorterLoader.hpp"
#include "Ordering/ChannelPositionMetadata.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

char const * kPoly2Config =
        "poly2\n"
        "1 1 21.65 25\n"
        "2 2 21.65 725\n"
        "3 3 21.65 125\n"
        "4 4 21.65 325\n"
        "5 5 21.65 375\n"
        "6 6 21.65 475\n"
        "7 7 21.65 675\n"
        "8 8 21.65 775\n"
        "9 9 21.65 225\n"
        "10 10 21.65 275\n"
        "11 11 21.65 575\n"
        "12 12 21.65 525\n"
        "13 13 21.65 425\n"
        "14 14 21.65 175\n"
        "15 15 21.65 625\n"
        "16 16 21.65 75\n"
        "17 17 -21.65 0\n"
        "18 18 -21.65 450\n"
        "19 19 -21.65 100\n"
        "20 20 -21.65 250\n"
        "21 21 -21.65 600\n"
        "22 22 -21.65 400\n"
        "23 23 -21.65 550\n"
        "24 24 -21.65 750\n"
        "25 25 -21.65 200\n"
        "26 26 -21.65 650\n"
        "27 27 -21.65 350\n"
        "28 28 -21.65 500\n"
        "29 29 -21.65 700\n"
        "30 30 -21.65 150\n"
        "31 31 -21.65 300\n"
        "32 32 -21.65 50\n";

char const * kPoly3ConfigSnippet =
        "poly3\n"
        "7 7 18, 187.5\n"
        "8 8 18, 237.5\n"
        "20 20 0 0\n";

[[nodiscard]] std::vector<std::string> makeVoltageKeys(int count) {
    std::vector<std::string> keys;
    keys.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        keys.push_back("voltage_" + std::to_string(i));
    }
    return keys;
}

[[nodiscard]] std::vector<std::string> makeAnalogKeys(int count) {
    std::vector<std::string> keys;
    keys.reserve(static_cast<size_t>(count));
    for (int i = 1; i <= count; ++i) {
        keys.push_back("analog_" + std::to_string(i));
    }
    return keys;
}

[[nodiscard]] ChannelPositionMap makeConfigMap(
        std::string const & group,
        std::vector<ChannelPosition> const & positions) {
    ChannelPositionMap configs;
    configs[group] = positions;
    return configs;
}

}// namespace

TEST_CASE("parseSwindaleSpikeSorterConfig parses poly2 fixture", "[DataViewer][SpikeSorter]") {
    auto const positions = parseSwindaleSpikeSorterConfig(kPoly2Config);
    REQUIRE(positions.size() == 32);
    REQUIRE(positions.front().channel_id == 0);
    REQUIRE(positions.front().y == 25.0f);
    REQUIRE(positions.back().channel_id == 31);
    REQUIRE(positions.back().y == 50.0f);
}

TEST_CASE("parseSwindaleSpikeSorterConfig parses comma-separated poly3 values", "[DataViewer][SpikeSorter]") {
    auto const positions = parseSwindaleSpikeSorterConfig(kPoly3ConfigSnippet);
    REQUIRE(positions.size() == 3);

    REQUIRE(positions[0].channel_id == 6);
    REQUIRE(positions[0].y == 187.5f);
    REQUIRE(positions[1].channel_id == 7);
    REQUIRE(positions[1].y == 237.5f);
    REQUIRE(positions[2].channel_id == 19);
    REQUIRE(positions[2].y == 0.0f);
}

TEST_CASE("parseSeriesIdentity assigns group for 0-based suffix keys", "[DataViewer][SpikeSorter]") {
    auto const identity = parseSeriesIdentity("voltage_0");
    REQUIRE(identity.group == "voltage");
    REQUIRE_FALSE(identity.channel_id.has_value());
}

TEST_CASE("detectKeyOneBased distinguishes 0-based and 1-based suffixes", "[DataViewer][SpikeSorter]") {
    auto const zero_based = makeVoltageKeys(4);
    REQUIRE_FALSE(detectKeyOneBased(zero_based, "voltage"));

    auto const one_based = makeAnalogKeys(4);
    REQUIRE(detectKeyOneBased(one_based, "analog"));
}

TEST_CASE("extractChannelFromSeriesKey maps suffixes for 0-based and 1-based keys", "[DataViewer][SpikeSorter]") {
    REQUIRE(extractChannelFromSeriesKey("voltage_0", "voltage", false) == 0);
    REQUIRE(extractChannelFromSeriesKey("voltage_1", "voltage", false) == 1);
    REQUIRE(extractChannelFromSeriesKey("analog_1", "analog", true) == 0);
    REQUIRE(extractChannelFromSeriesKey("analog_2", "analog", true) == 1);
}

TEST_CASE("buildSwindaleSpikeSorterRanks assigns distinct ranks to voltage_0 and voltage_1", "[DataViewer][SpikeSorter]") {
    auto const positions = parseSwindaleSpikeSorterConfig(kPoly2Config);
    auto const configs = makeConfigMap("voltage", positions);
    auto const keys = makeVoltageKeys(4);

    auto const ranks = buildSwindaleSpikeSorterRanks(keys, configs, false);
    REQUIRE(ranks.at("voltage_0") != ranks.at("voltage_1"));
}

TEST_CASE("orderKeysBySwindaleSpikeSorter sorts voltage_0..31 by ascending poly2 Y", "[DataViewer][SpikeSorter]") {
    auto const positions = parseSwindaleSpikeSorterConfig(kPoly2Config);
    auto const configs = makeConfigMap("voltage", positions);
    auto const keys = makeVoltageKeys(32);

    std::vector<std::string> const expected{
            "voltage_16",
            "voltage_0",
            "voltage_31",
            "voltage_15",
            "voltage_18",
            "voltage_2",
            "voltage_29",
            "voltage_13",
            "voltage_24",
            "voltage_8",
            "voltage_19",
            "voltage_9",
            "voltage_30",
            "voltage_3",
            "voltage_26",
            "voltage_4",
            "voltage_21",
            "voltage_12",
            "voltage_17",
            "voltage_5",
            "voltage_27",
            "voltage_11",
            "voltage_22",
            "voltage_10",
            "voltage_20",
            "voltage_14",
            "voltage_25",
            "voltage_6",
            "voltage_28",
            "voltage_1",
            "voltage_23",
            "voltage_7",
    };

    auto const ordered = orderKeysBySwindaleSpikeSorter(keys, configs, false);
    REQUIRE(ordered == expected);
}

TEST_CASE("orderKeysBySwindaleSpikeSorter sorts analog_1..4 by ascending Y with 1-based keys", "[DataViewer][SpikeSorter]") {
    char const * cfg =
            "poly2\n"
            "1 1 0 300\n"
            "2 2 0 100\n"
            "3 3 0 200\n"
            "4 4 0 400\n";

    auto const positions = parseSwindaleSpikeSorterConfig(cfg);
    auto const configs = makeConfigMap("analog", positions);
    auto const keys = makeAnalogKeys(4);

    auto const ordered = orderKeysBySwindaleSpikeSorter(keys, configs, true);
    REQUIRE(ordered.size() == 4);
    REQUIRE(ordered[0] == "analog_2");
    REQUIRE(ordered[1] == "analog_3");
    REQUIRE(ordered[2] == "analog_1");
    REQUIRE(ordered[3] == "analog_4");
}

TEST_CASE("buildSwindaleSpikeSorterRanks mis-orders 0-based keys when key_one_based is true", "[DataViewer][SpikeSorter]") {
    auto const positions = parseSwindaleSpikeSorterConfig(kPoly2Config);
    auto const configs = makeConfigMap("voltage", positions);
    auto const keys = makeVoltageKeys(4);

    auto const ordered_wrong = orderKeysBySwindaleSpikeSorter(keys, configs, true);
    auto const ordered_correct = orderKeysBySwindaleSpikeSorter(keys, configs, false);
    REQUIRE(ordered_wrong != ordered_correct);
    REQUIRE(ordered_correct[0] == "voltage_0");
    REQUIRE(ordered_correct[1] == "voltage_2");
    REQUIRE(ordered_wrong[0] == "voltage_0");
    REQUIRE(ordered_wrong[1] == "voltage_1");
}
