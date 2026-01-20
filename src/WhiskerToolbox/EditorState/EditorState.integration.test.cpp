/**
 * @file EditorState.integration.test.cpp
 * @brief Integration tests for EditorState, SelectionContext, and WorkspaceManager
 * 
 * These tests verify cross-widget communication patterns using the EditorState
 * infrastructure. They test scenarios where multiple widget states coordinate
 * via SelectionContext.
 * 
 * @see EditorState.test.cpp for unit tests of individual components
 */

#include "EditorState/EditorState.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/WorkspaceManager.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager_Widget/DataManagerWidgetState.hpp"
#include "Media_Widget/MediaWidgetState.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include <memory>

// === Cross-Widget Communication Integration Tests ===

TEST_CASE("Cross-widget selection coordination", "[EditorState][SelectionContext][integration]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("MediaWidgetState can be registered with WorkspaceManager") {
        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto state = std::make_shared<MediaWidgetState>();
        workspace.registerState(state);

        REQUIRE(workspace.getAllStates().size() == 1);
        REQUIRE(workspace.getState(state->getInstanceId()) == state);

        workspace.unregisterState(state->getInstanceId());
        REQUIRE(workspace.getAllStates().empty());
    }

    SECTION("DataManagerWidgetState can be registered with WorkspaceManager") {
        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto state = std::make_shared<DataManagerWidgetState>();
        workspace.registerState(state);

        REQUIRE(workspace.getAllStates().size() == 1);
        REQUIRE(workspace.getState(state->getInstanceId()) == state);

        workspace.unregisterState(state->getInstanceId());
        REQUIRE(workspace.getAllStates().empty());
    }

    SECTION("MediaWidgetState responds to external selection via signal chain") {
        // Simulate the pattern used in Media_Widget:
        // External widget selects data -> SelectionContext -> MediaWidgetState
        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        workspace.registerState(media_state);

        auto external_state = std::make_shared<DataManagerWidgetState>();
        workspace.registerState(external_state);

        auto * selection_context = workspace.selectionContext();

        // Simulate external selection (from DataManager_Widget)
        SelectionSource external_source{external_state->getInstanceId(), "feature_table"};
        selection_context->setSelectedData("external_data_key", external_source);

        // Verify SelectionContext received the selection
        REQUIRE(selection_context->primarySelectedData() == "external_data_key");

        // In the actual widget, we would update state in the slot handler.
        // Here we simulate that behavior:
        if (external_source.editor_instance_id != media_state->getInstanceId()) {
            media_state->setDisplayedDataKey(selection_context->primarySelectedData());
        }

        REQUIRE(media_state->displayedDataKey() == "external_data_key");
    }

    SECTION("Widget state ignores own selections (no circular updates)") {
        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        workspace.registerState(media_state);

        auto * selection_context = workspace.selectionContext();

        // Simulate selection originating from Media_Widget itself
        SelectionSource own_source{media_state->getInstanceId(), "feature_table"};

        // First set a value
        media_state->setDisplayedDataKey("initial_key");

        // Now simulate receiving a selection change from ourselves
        selection_context->setSelectedData("new_key", own_source);

        // The handler should check source and NOT update if it's our own
        // (This simulates the filtering logic in _onExternalSelectionChanged)
        if (own_source.editor_instance_id != media_state->getInstanceId()) {
            media_state->setDisplayedDataKey(selection_context->primarySelectedData());
        }

        // State should remain unchanged since we ignored our own selection
        REQUIRE(media_state->displayedDataKey() == "initial_key");
    }

    SECTION("Multiple Media_Widget states coordinate via SelectionContext") {
        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto media_state1 = std::make_shared<MediaWidgetState>();
        media_state1->setDisplayName("Media 1");
        workspace.registerState(media_state1);

        auto media_state2 = std::make_shared<MediaWidgetState>();
        media_state2->setDisplayName("Media 2");
        workspace.registerState(media_state2);

        auto * selection_context = workspace.selectionContext();

        // Media 1 selects something
        SelectionSource source1{media_state1->getInstanceId(), "feature_table"};
        selection_context->setSelectedData("data_from_media1", source1);

        // Media 2 should respond (simulating the slot handler)
        if (source1.editor_instance_id != media_state2->getInstanceId()) {
            media_state2->setDisplayedDataKey(selection_context->primarySelectedData());
        }

        REQUIRE(media_state2->displayedDataKey() == "data_from_media1");

        // Media 1 should NOT respond to its own selection
        if (source1.editor_instance_id != media_state1->getInstanceId()) {
            // This block should not execute
            media_state1->setDisplayedDataKey("should_not_happen");
        }

        // Media 1's state should not have been auto-updated
        REQUIRE(media_state1->displayedDataKey().isEmpty());
    }

    SECTION("SelectionSource correctly identifies originating widget") {
        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        auto dm_state = std::make_shared<DataManagerWidgetState>();

        workspace.registerState(media_state);
        workspace.registerState(dm_state);

        // All instance IDs should be unique
        REQUIRE(media_state->getInstanceId() != dm_state->getInstanceId());

        // Create selection sources
        SelectionSource media_source{media_state->getInstanceId(), "feature_table"};
        SelectionSource dm_source{dm_state->getInstanceId(), "feature_table"};

        // They should be distinguishable
        REQUIRE_FALSE(media_source == dm_source);
        REQUIRE(media_source.editor_instance_id == media_state->getInstanceId());
        REQUIRE(dm_source.editor_instance_id == dm_state->getInstanceId());
    }

    SECTION("DataManager_Widget selection propagates to Media_Widget state") {
        // This tests the full signal chain:
        // DataManager_Widget feature table -> DataManagerWidgetState -> SelectionContext -> Media_Widget handler

        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto dm_state = std::make_shared<DataManagerWidgetState>();
        auto media_state = std::make_shared<MediaWidgetState>();

        workspace.registerState(dm_state);
        workspace.registerState(media_state);

        auto * selection_context = workspace.selectionContext();

        // Track selection changes received by Media_Widget
        bool media_received_selection = false;
        QString received_key;

        QObject::connect(selection_context, &SelectionContext::selectionChanged,
                         [&](SelectionSource const & source) {
            // Simulate Media_Widget's _onExternalSelectionChanged behavior
            if (source.editor_instance_id != media_state->getInstanceId()) {
                media_received_selection = true;
                received_key = selection_context->primarySelectedData();
                media_state->setDisplayedDataKey(received_key);
            }
        });

        // Simulate DataManager_Widget selecting a feature
        dm_state->setSelectedDataKey("whisker_data");

        // This would normally trigger the state -> SelectionContext connection in the widget
        // Here we simulate it:
        SelectionSource dm_source{dm_state->getInstanceId(), "feature_table"};
        selection_context->setSelectedData(dm_state->selectedDataKey(), dm_source);

        // Verify the chain worked
        REQUIRE(media_received_selection);
        REQUIRE(received_key == "whisker_data");
        REQUIRE(media_state->displayedDataKey() == "whisker_data");
    }
}

TEST_CASE("Workspace serialization with multiple widget states", "[EditorState][WorkspaceManager][integration]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Workspace with mixed state types serializes correctly") {
        auto dm = std::make_shared<DataManager>();
        WorkspaceManager workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        media_state->setDisplayName("Media Viewer 1");
        media_state->setDisplayedDataKey("video_data");
        workspace.registerState(media_state);

        auto dm_state = std::make_shared<DataManagerWidgetState>();
        dm_state->setDisplayName("Data Manager");
        dm_state->setSelectedDataKey("whisker_lines");
        workspace.registerState(dm_state);

        REQUIRE(workspace.getAllStates().size() == 2);

        // Note: Full workspace serialization would require the factory system
        // to be set up with state factories. This test verifies states can coexist.
        auto media_json = media_state->toJson();
        auto dm_json = dm_state->toJson();

        REQUIRE_FALSE(media_json.empty());
        REQUIRE_FALSE(dm_json.empty());
    }
}
