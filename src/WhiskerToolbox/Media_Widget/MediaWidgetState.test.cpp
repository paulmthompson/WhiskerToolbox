#include "MediaWidgetState.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include <string>

// === MediaWidgetState Tests ===

TEST_CASE("MediaWidgetState basics", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Instance ID is unique") {
        MediaWidgetState state1;
        MediaWidgetState state2;

        REQUIRE_FALSE(state1.getInstanceId().isEmpty());
        REQUIRE_FALSE(state2.getInstanceId().isEmpty());
        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }

    SECTION("Type name is correct") {
        MediaWidgetState state;
        REQUIRE(state.getTypeName() == "MediaWidget");
    }

    SECTION("Display name defaults and can be set") {
        MediaWidgetState state;
        REQUIRE(state.getDisplayName() == "Media Viewer");

        state.setDisplayName("Custom Media");
        REQUIRE(state.getDisplayName() == "Custom Media");
    }

    SECTION("Dirty state tracking") {
        MediaWidgetState state;
        REQUIRE_FALSE(state.isDirty());

        state.setDisplayedDataKey("video_data");
        REQUIRE(state.isDirty());

        state.markClean();
        REQUIRE_FALSE(state.isDirty());
    }

    SECTION("Displayed data key management") {
        MediaWidgetState state;
        REQUIRE(state.displayedDataKey().isEmpty());

        state.setDisplayedDataKey("video_data");
        REQUIRE(state.displayedDataKey() == "video_data");

        state.setDisplayedDataKey("");
        REQUIRE(state.displayedDataKey().isEmpty());
    }
}

TEST_CASE("MediaWidgetState serialization", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Round-trip serialization") {
        MediaWidgetState original;
        original.setDisplayName("Test Media");
        original.setDisplayedDataKey("image_data");

        auto json = original.toJson();

        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.displayedDataKey() == "image_data");
        REQUIRE(restored.getDisplayName() == "Test Media");
    }

    SECTION("Instance ID is preserved across serialization") {
        MediaWidgetState original;
        QString original_id = original.getInstanceId();
        original.setDisplayedDataKey("test");

        auto json = original.toJson();

        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getInstanceId() == original_id);
    }

    SECTION("Invalid JSON returns false") {
        MediaWidgetState state;
        REQUIRE_FALSE(state.fromJson("not valid json"));
        REQUIRE_FALSE(state.fromJson("{\"invalid\": \"schema\"}"));
    }

    SECTION("Empty state serializes correctly") {
        MediaWidgetState state;
        auto json = state.toJson();
        REQUIRE_FALSE(json.empty());

        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.displayedDataKey().isEmpty());
        REQUIRE(restored.getDisplayName() == "Media Viewer");
    }
}

TEST_CASE("MediaWidgetState signals", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("stateChanged emitted on modification") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &EditorState::stateChanged);

        state.setDisplayedDataKey("data1");
        REQUIRE(spy.count() == 1);

        state.setDisplayedDataKey("data2");
        REQUIRE(spy.count() == 2);
    }

    SECTION("displayedDataKeyChanged emitted on key change") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::displayedDataKeyChanged);

        state.setDisplayedDataKey("video1");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "video1");

        // Same value should not emit
        state.setDisplayedDataKey("video1");
        REQUIRE(spy.count() == 1);

        state.setDisplayedDataKey("video2");
        REQUIRE(spy.count() == 2);
        REQUIRE(spy.at(1).at(0).toString() == "video2");
    }

    SECTION("dirtyChanged emitted appropriately") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &EditorState::dirtyChanged);

        state.setDisplayedDataKey("data1");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toBool() == true);

        state.setDisplayedDataKey("data2");// Already dirty, no new signal
        REQUIRE(spy.count() == 1);

        state.markClean();
        REQUIRE(spy.count() == 2);
        REQUIRE(spy.at(1).at(0).toBool() == false);
    }

    SECTION("displayNameChanged emitted") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &EditorState::displayNameChanged);

        state.setDisplayName("New Name");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "New Name");
    }
}
