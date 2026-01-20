#include "DataManagerWidgetState.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include <string>

// === DataManagerWidgetState Tests ===

TEST_CASE("DataManagerWidgetState basics", "[DataManagerWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Instance ID is unique") {
        DataManagerWidgetState state1;
        DataManagerWidgetState state2;

        REQUIRE_FALSE(state1.getInstanceId().isEmpty());
        REQUIRE_FALSE(state2.getInstanceId().isEmpty());
        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }

    SECTION("Type name is correct") {
        DataManagerWidgetState state;
        REQUIRE(state.getTypeName() == "DataManagerWidget");
    }

    SECTION("Display name defaults and can be set") {
        DataManagerWidgetState state;
        REQUIRE(state.getDisplayName() == "Data Manager");

        state.setDisplayName("Custom Name");
        REQUIRE(state.getDisplayName() == "Custom Name");
    }

    SECTION("Dirty state tracking") {
        DataManagerWidgetState state;
        REQUIRE_FALSE(state.isDirty());

        state.setSelectedDataKey("test_key");
        REQUIRE(state.isDirty());

        state.markClean();
        REQUIRE_FALSE(state.isDirty());
    }

    SECTION("Selected data key management") {
        DataManagerWidgetState state;
        REQUIRE(state.selectedDataKey().isEmpty());

        state.setSelectedDataKey("my_data_key");
        REQUIRE(state.selectedDataKey() == "my_data_key");

        state.setSelectedDataKey("");
        REQUIRE(state.selectedDataKey().isEmpty());
    }
}

TEST_CASE("DataManagerWidgetState serialization", "[DataManagerWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Round-trip serialization") {
        DataManagerWidgetState original;
        original.setDisplayName("Test State");
        original.setSelectedDataKey("selected_key");

        auto json = original.toJson();

        DataManagerWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.selectedDataKey() == "selected_key");
        REQUIRE(restored.getDisplayName() == "Test State");
    }

    SECTION("Instance ID is preserved across serialization") {
        DataManagerWidgetState original;
        QString original_id = original.getInstanceId();
        original.setSelectedDataKey("test");

        auto json = original.toJson();

        DataManagerWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getInstanceId() == original_id);
    }

    SECTION("Invalid JSON returns false") {
        DataManagerWidgetState state;
        REQUIRE_FALSE(state.fromJson("not valid json"));
        REQUIRE_FALSE(state.fromJson("{\"invalid\": \"schema\"}"));
    }

    SECTION("Empty state serializes correctly") {
        DataManagerWidgetState state;
        auto json = state.toJson();
        REQUIRE_FALSE(json.empty());

        DataManagerWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.selectedDataKey().isEmpty());
        REQUIRE(restored.getDisplayName() == "Data Manager");
    }
}

TEST_CASE("DataManagerWidgetState signals", "[DataManagerWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("stateChanged emitted on modification") {
        DataManagerWidgetState state;
        QSignalSpy spy(&state, &EditorState::stateChanged);

        state.setSelectedDataKey("key1");
        REQUIRE(spy.count() == 1);

        state.setSelectedDataKey("key2");
        REQUIRE(spy.count() == 2);
    }

    SECTION("selectedDataKeyChanged emitted on key change") {
        DataManagerWidgetState state;
        QSignalSpy spy(&state, &DataManagerWidgetState::selectedDataKeyChanged);

        state.setSelectedDataKey("key1");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "key1");

        // Same value should not emit
        state.setSelectedDataKey("key1");
        REQUIRE(spy.count() == 1);

        state.setSelectedDataKey("key2");
        REQUIRE(spy.count() == 2);
        REQUIRE(spy.at(1).at(0).toString() == "key2");
    }

    SECTION("dirtyChanged emitted appropriately") {
        DataManagerWidgetState state;
        QSignalSpy spy(&state, &EditorState::dirtyChanged);

        state.setSelectedDataKey("key1");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toBool() == true);

        state.setSelectedDataKey("key2");// Already dirty, no new signal
        REQUIRE(spy.count() == 1);

        state.markClean();
        REQUIRE(spy.count() == 2);
        REQUIRE(spy.at(1).at(0).toBool() == false);
    }

    SECTION("displayNameChanged emitted") {
        DataManagerWidgetState state;
        QSignalSpy spy(&state, &EditorState::displayNameChanged);

        state.setDisplayName("New Name");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "New Name");
    }
}
