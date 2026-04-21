/**
 * @file TriageSessionState.test.cpp
 * @brief Unit tests for TriageSessionState slot serialization and migration behavior
 */

#include <catch2/catch_test_macros.hpp>

#include "Commands/Core/CommandDescriptor.hpp"
#include "TriageSession_Widget/Core/TriageSessionState.hpp"

#include <rfl/json.hpp>

#include <memory>
#include <string>

namespace {

TriageHotkeySlotData makeSlot(
        int const slot_index,
        std::string display_name,
        bool const enabled,
        std::optional<std::string> sequence_json) {
    TriageHotkeySlotData slot;
    slot.slot_index = slot_index;
    slot.display_name = std::move(display_name);
    slot.enabled = enabled;
    slot.sequence_json = std::move(sequence_json);
    return slot;
}

}// namespace

TEST_CASE("TriageSessionState initializes 9 default sequence slots", "[TriageSessionState]") {
    TriageSessionState state(nullptr);

    auto const & slot_data = state.sequenceSlots();
    REQUIRE(slot_data.size() == static_cast<std::size_t>(TriageSessionState::sequenceSlotCount()));

    for (int i = 1; i <= TriageSessionState::sequenceSlotCount(); ++i) {
        auto slot = state.sequenceSlot(i);
        REQUIRE(slot.has_value());
        CHECK(slot->slot_index == i);
        CHECK(slot->display_name.empty());
        CHECK(slot->enabled);
        CHECK_FALSE(slot->sequence_json.has_value());
    }
}

TEST_CASE("TriageSessionState roundtrips all 9 slots", "[TriageSessionState]") {
    TriageSessionState original(nullptr);
    original.setDisplayName(QStringLiteral("Triage Slots"));
    original.setTrackedRegionsKey("tracked_custom");

    for (int i = 1; i <= TriageSessionState::sequenceSlotCount(); ++i) {
        auto slot = makeSlot(
                i,
                std::string("Slot ") + std::to_string(i),
                (i % 2) == 1,
                (i % 3) == 0
                        ? std::optional<std::string>(
                                  std::string("{\"name\":\"slot_") + std::to_string(i) + "\",\"commands\":[]}")
                        : std::nullopt);
        REQUIRE(original.setSequenceSlot(i, slot));
    }

    auto const json = original.toJson();

    TriageSessionState restored(nullptr);
    REQUIRE(restored.fromJson(json));

    CHECK(restored.getDisplayName() == QStringLiteral("Triage Slots"));
    CHECK(restored.trackedRegionsKey() == "tracked_custom");

    for (int i = 1; i <= TriageSessionState::sequenceSlotCount(); ++i) {
        auto left = original.sequenceSlot(i);
        auto right = restored.sequenceSlot(i);
        REQUIRE(left.has_value());
        REQUIRE(right.has_value());
        CHECK(right->slot_index == left->slot_index);
        CHECK(right->display_name == left->display_name);
        CHECK(right->enabled == left->enabled);
        CHECK(right->sequence_json == left->sequence_json);
    }
}

TEST_CASE("TriageSessionState migrates legacy pipeline_json into slot 1", "[TriageSessionState][migration]") {
    TriageSessionStateData legacy;
    legacy.instance_id = "legacy-triage";
    legacy.display_name = "Legacy Triage";
    legacy.pipeline_json = std::string("{\"name\":\"legacy_pipeline\",\"commands\":[]}");
    legacy.tracked_regions_key = "tracked_legacy";
    // sequence_slots intentionally empty to trigger migration

    auto const legacy_json = rfl::json::write(legacy);

    TriageSessionState migrated(nullptr);
    REQUIRE(migrated.fromJson(legacy_json));

    REQUIRE(migrated.sequenceSlots().size() ==
            static_cast<std::size_t>(TriageSessionState::sequenceSlotCount()));

    auto slot1 = migrated.sequenceSlot(1);
    REQUIRE(slot1.has_value());
    REQUIRE(slot1->sequence_json.has_value());
    CHECK(*slot1->sequence_json == *legacy.pipeline_json);

    for (int i = 2; i <= TriageSessionState::sequenceSlotCount(); ++i) {
        auto slot = migrated.sequenceSlot(i);
        REQUIRE(slot.has_value());
        CHECK_FALSE(slot->sequence_json.has_value());
    }

    CHECK(migrated.trackedRegionsKey() == "tracked_legacy");
    CHECK(migrated.getDisplayName() == QStringLiteral("Legacy Triage"));
}

TEST_CASE("TriageSessionState does not override explicit slot data with legacy pipeline_json",
          "[TriageSessionState][migration]") {
    TriageSessionStateData mixed;
    mixed.instance_id = "mixed-state";
    mixed.pipeline_json = std::string("{\"name\":\"legacy_pipeline\",\"commands\":[]}");
    mixed.sequence_slots.resize(static_cast<std::size_t>(TriageSessionState::sequenceSlotCount()));

    for (int i = 1; i <= TriageSessionState::sequenceSlotCount(); ++i) {
        mixed.sequence_slots[static_cast<std::size_t>(i - 1)] = makeSlot(i, "", true, std::nullopt);
    }
    mixed.sequence_slots[0].sequence_json = std::string("{\"name\":\"explicit_slot_1\",\"commands\":[]}");

    auto const mixed_json = rfl::json::write(mixed);

    TriageSessionState restored(nullptr);
    REQUIRE(restored.fromJson(mixed_json));

    auto slot1 = restored.sequenceSlot(1);
    REQUIRE(slot1.has_value());
    REQUIRE(slot1->sequence_json.has_value());
    CHECK(*slot1->sequence_json == "{\"name\":\"explicit_slot_1\",\"commands\":[]}");
}

TEST_CASE("TriageSessionState rejects out-of-range slot writes", "[TriageSessionState]") {
    TriageSessionState state(nullptr);

    CHECK_FALSE(state.setSequenceSlot(0, makeSlot(0, "bad", true, std::nullopt)));
    CHECK_FALSE(state.setSequenceSlot(TriageSessionState::sequenceSlotCount() + 1,
                                      makeSlot(10, "bad", true, std::nullopt)));
    CHECK_FALSE(state.setSequenceSlotJson(0, std::string("{}")));
    CHECK_FALSE(state.setSequenceSlotJson(TriageSessionState::sequenceSlotCount() + 1,
                                          std::string("{}")));
}

TEST_CASE("TriageSessionState empty slot JSON is treated as no payload",
          "[TriageSessionState][slot-json]") {
    TriageSessionState state(nullptr);

    auto slot = state.sequenceSlot(4);
    REQUIRE(slot.has_value());
    CHECK_FALSE(slot->sequence_json.has_value());
}

TEST_CASE("TriageSessionState invalid slot JSON fails CommandSequenceDescriptor parsing",
          "[TriageSessionState][slot-json]") {
    TriageSessionState state(nullptr);
    REQUIRE(state.setSequenceSlotJson(2, std::string("{not valid json")));

    auto slot = state.sequenceSlot(2);
    REQUIRE(slot.has_value());
    REQUIRE(slot->sequence_json.has_value());

    auto parsed = rfl::json::read<commands::CommandSequenceDescriptor>(*slot->sequence_json);
    CHECK_FALSE(parsed);
}
