/**
 * @file OrderingPolicyResolver.test.cpp
 * @brief Unit tests for DataViewer ordering policy resolver precedence and diagnostics.
 */

#include "DataViewer_Widget/Core/OrderingPolicyResolver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

namespace {

using ::SortableRankMap;
using DataViewer::OrderingDiagnosticReason;
using DataViewer::OrderingInputItem;
using DataViewer::resolveOrdering;

OrderingInputItem makeItem(std::string key,
                           CorePlotting::SeriesType type,
                           std::string group,
                           std::optional<int> channel,
                           std::optional<int> explicit_lane_order,
                           int insertion_index) {
    OrderingInputItem item;
    item.key = std::move(key);
    item.type = type;
    item.identity.group = std::move(group);
    item.identity.channel_id = channel;
    item.explicit_lane_order = explicit_lane_order;
    item.insertion_index = insertion_index;
    return item;
}

}// namespace

TEST_CASE("OrderingPolicyResolver explicit lane_order takes precedence", "[DataViewer][OrderingResolver]") {
    std::vector<OrderingInputItem> input{
            makeItem("analog_1", CorePlotting::SeriesType::Analog, "analog", 0, 20, 0),
            makeItem("analog_2", CorePlotting::SeriesType::Analog, "analog", 1, 10, 1),
            makeItem("event_1", CorePlotting::SeriesType::DigitalEvent, "event", 0, std::nullopt, 2)};

    SortableRankMap fallback_ranks;
    fallback_ranks["analog_1"] = 0;
    fallback_ranks["analog_2"] = 1;

    auto const resolution = resolveOrdering(input, fallback_ranks);
    REQUIRE(resolution.ordered_input_indices.size() == 3);

    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[0])].key == "analog_2");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[1])].key == "analog_1");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[2])].key == "event_1");

    REQUIRE(resolution.diagnostics[0].reason == OrderingDiagnosticReason::ExplicitLaneOrder);
    REQUIRE(resolution.diagnostics[1].reason == OrderingDiagnosticReason::ExplicitLaneOrder);
}

TEST_CASE("OrderingPolicyResolver uses fallback sortable ranks", "[DataViewer][OrderingResolver]") {
    std::vector<OrderingInputItem> input{
            makeItem("analog_1", CorePlotting::SeriesType::Analog, "analog", 0, std::nullopt, 0),
            makeItem("analog_2", CorePlotting::SeriesType::Analog, "analog", 1, std::nullopt, 1)};

    SortableRankMap fallback_ranks;
    fallback_ranks["analog_1"] = 10;
    fallback_ranks["analog_2"] = 5;

    auto const resolution = resolveOrdering(input, fallback_ranks);
    REQUIRE(resolution.ordered_input_indices.size() == 2);

    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[0])].key == "analog_2");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[1])].key == "analog_1");

    REQUIRE(resolution.diagnostics[0].reason == OrderingDiagnosticReason::FallbackSortableRank);
    REQUIRE(resolution.diagnostics[1].reason == OrderingDiagnosticReason::FallbackSortableRank);
}

TEST_CASE("OrderingPolicyResolver deterministic tie-break is stable", "[DataViewer][OrderingResolver]") {
    std::vector<OrderingInputItem> input{
            makeItem("z_series", CorePlotting::SeriesType::Analog, "group_b", 2, std::nullopt, 0),
            makeItem("a_series", CorePlotting::SeriesType::DigitalEvent, "group_a", 0, std::nullopt, 1),
            makeItem("b_series", CorePlotting::SeriesType::Analog, "group_a", 1, std::nullopt, 2)};

    SortableRankMap const fallback_ranks;

    auto const resolution = resolveOrdering(input, fallback_ranks);
    REQUIRE(resolution.ordered_input_indices.size() == 3);

    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[0])].key == "a_series");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[1])].key == "b_series");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[2])].key == "z_series");

    REQUIRE(resolution.diagnostics[0].reason == OrderingDiagnosticReason::DeterministicTieBreak);
    REQUIRE(resolution.diagnostics[1].reason == OrderingDiagnosticReason::DeterministicTieBreak);
    REQUIRE(resolution.diagnostics[2].reason == OrderingDiagnosticReason::DeterministicTieBreak);
}

TEST_CASE("OrderingPolicyResolver applies relational constraints before fallback", "[DataViewer][OrderingResolver]") {
    std::vector<OrderingInputItem> input{
            makeItem("analog_7", CorePlotting::SeriesType::Analog, "analog", 7, std::nullopt, 0),
            makeItem("event_2", CorePlotting::SeriesType::DigitalEvent, "event", 2, std::nullopt, 1),
            makeItem("event_3", CorePlotting::SeriesType::DigitalEvent, "event", 3, std::nullopt, 2)};

    SortableRankMap fallback_ranks;
    fallback_ranks["analog_7"] = 0;
    fallback_ranks["event_2"] = 50;
    fallback_ranks["event_3"] = 60;

    std::vector<DataViewer::OrderingConstraint> const constraints{
            DataViewer::OrderingConstraint{"event_2", "analog_7"},
            DataViewer::OrderingConstraint{"event_3", "analog_7"}};

    auto const resolution = resolveOrdering(input, fallback_ranks, constraints);
    REQUIRE(resolution.ordered_input_indices.size() == 3);

    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[0])].key == "event_2");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[1])].key == "event_3");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[2])].key == "analog_7");

    REQUIRE(resolution.diagnostics[0].reason == OrderingDiagnosticReason::RelationalConstraint);
    REQUIRE(resolution.diagnostics[1].reason == OrderingDiagnosticReason::RelationalConstraint);
    REQUIRE(resolution.diagnostics[2].reason == OrderingDiagnosticReason::RelationalConstraint);
}

TEST_CASE("OrderingPolicyResolver explicit lane order wins over conflicting constraints", "[DataViewer][OrderingResolver]") {
    std::vector<OrderingInputItem> input{
            makeItem("analog_1", CorePlotting::SeriesType::Analog, "analog", 1, 1, 0),
            makeItem("event_1", CorePlotting::SeriesType::DigitalEvent, "event", 1, std::nullopt, 1)};

    SortableRankMap const fallback_ranks;
    std::vector<DataViewer::OrderingConstraint> const constraints{
            DataViewer::OrderingConstraint{"event_1", "analog_1"}};

    auto const resolution = resolveOrdering(input, fallback_ranks, constraints);
    REQUIRE(resolution.ordered_input_indices.size() == 2);

    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[0])].key == "analog_1");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[1])].key == "event_1");
    REQUIRE(resolution.diagnostics[0].reason == OrderingDiagnosticReason::ExplicitLaneOrder);
}

TEST_CASE("OrderingPolicyResolver breaks cycles deterministically", "[DataViewer][OrderingResolver]") {
    std::vector<OrderingInputItem> input{
            makeItem("a", CorePlotting::SeriesType::Analog, "g", 0, std::nullopt, 0),
            makeItem("b", CorePlotting::SeriesType::Analog, "g", 1, std::nullopt, 1),
            makeItem("c", CorePlotting::SeriesType::Analog, "g", 2, std::nullopt, 2)};

    SortableRankMap const fallback_ranks;
    std::vector<DataViewer::OrderingConstraint> const constraints{
            DataViewer::OrderingConstraint{"a", "b"},
            DataViewer::OrderingConstraint{"b", "c"},
            DataViewer::OrderingConstraint{"c", "a"}};

    auto const resolution = resolveOrdering(input, fallback_ranks, constraints);
    REQUIRE(resolution.ordered_input_indices.size() == 3);

    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[0])].key == "a");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[1])].key == "b");
    REQUIRE(input[static_cast<size_t>(resolution.ordered_input_indices[2])].key == "c");

    bool has_cycle_break = false;
    for (auto const & diagnostic: resolution.diagnostics) {
        if (diagnostic.reason == OrderingDiagnosticReason::ConstraintCycleBreak) {
            has_cycle_break = true;
            break;
        }
    }
    REQUIRE(has_cycle_break);
}
