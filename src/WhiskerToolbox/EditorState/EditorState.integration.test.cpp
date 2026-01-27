/**
 * @file EditorState.integration.test.cpp
 * @brief Integration tests for EditorState, SelectionContext, and EditorRegistry
 * 
 * These tests verify cross-widget communication patterns using the EditorState
 * infrastructure. They test scenarios where multiple widget states coordinate
 * via SelectionContext.
 * 
 * @see EditorState.test.cpp for unit tests of individual components
 */

#include "EditorState/EditorState.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/StrongTypes.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager_Widget/DataManagerWidgetState.hpp"
#include "DataTransform_Widget/DataTransformWidgetState.hpp"
#include "Media_Widget/Core/MediaWidgetState.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include <memory>

// === Cross-Widget Communication Integration Tests ===

TEST_CASE("Cross-widget selection coordination", "[EditorState][SelectionContext][integration]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("MediaWidgetState can be registered with EditorRegistry") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto state = std::make_shared<MediaWidgetState>();
        workspace.registerState(state);

        REQUIRE(workspace.allStates().size() == 1);
        REQUIRE(workspace.state(EditorInstanceId(state->getInstanceId())) == state);

        workspace.unregisterState(EditorInstanceId(state->getInstanceId()));
        REQUIRE(workspace.allStates().empty());
    }

    SECTION("DataManagerWidgetState can be registered with EditorRegistry") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto state = std::make_shared<DataManagerWidgetState>();
        workspace.registerState(state);

        REQUIRE(workspace.allStates().size() == 1);
        REQUIRE(workspace.state(EditorInstanceId(state->getInstanceId())) == state);

        workspace.unregisterState(EditorInstanceId(state->getInstanceId()));
        REQUIRE(workspace.allStates().empty());
    }

    SECTION("MediaWidgetState responds to external selection via signal chain") {
        // Simulate the pattern used in Media_Widget:
        // External widget selects data -> SelectionContext -> MediaWidgetState
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        workspace.registerState(media_state);

        auto external_state = std::make_shared<DataManagerWidgetState>();
        workspace.registerState(external_state);

        auto * selection_context = workspace.selectionContext();

        // Simulate external selection (from DataManager_Widget)
        SelectionSource external_source{EditorInstanceId(external_state->getInstanceId()), "feature_table"};
        selection_context->setSelectedData(SelectedDataKey("external_data_key"), external_source);

        // Verify SelectionContext received the selection
        REQUIRE(selection_context->primarySelectedData().toString() == "external_data_key");

        // In the actual widget, we would update state in the slot handler.
        // Here we simulate that behavior:
        if (external_source.editor_instance_id.toString() != media_state->getInstanceId()) {
            media_state->setDisplayedDataKey(selection_context->primarySelectedData().toString());
        }

        REQUIRE(media_state->displayedDataKey() == "external_data_key");
    }

    SECTION("Widget state ignores own selections (no circular updates)") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        workspace.registerState(media_state);

        auto * selection_context = workspace.selectionContext();

        // Simulate selection originating from Media_Widget itself
        SelectionSource own_source{EditorInstanceId(media_state->getInstanceId()), "feature_table"};

        // First set a value
        media_state->setDisplayedDataKey("initial_key");

        // Now simulate receiving a selection change from ourselves
        selection_context->setSelectedData(SelectedDataKey("new_key"), own_source);

        // The handler should check source and NOT update if it's our own
        // (This simulates the filtering logic in _onExternalSelectionChanged)
        if (own_source.editor_instance_id.toString() != media_state->getInstanceId()) {
            media_state->setDisplayedDataKey(selection_context->primarySelectedData().toString());
        }

        // State should remain unchanged since we ignored our own selection
        REQUIRE(media_state->displayedDataKey() == "initial_key");
    }

    SECTION("Multiple Media_Widget states coordinate via SelectionContext") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto media_state1 = std::make_shared<MediaWidgetState>();
        media_state1->setDisplayName("Media 1");
        workspace.registerState(media_state1);

        auto media_state2 = std::make_shared<MediaWidgetState>();
        media_state2->setDisplayName("Media 2");
        workspace.registerState(media_state2);

        auto * selection_context = workspace.selectionContext();

        // Media 1 selects something
        SelectionSource source1{EditorInstanceId(media_state1->getInstanceId()), "feature_table"};
        selection_context->setSelectedData(SelectedDataKey("data_from_media1"), source1);

        // Media 2 should respond (simulating the slot handler)
        if (source1.editor_instance_id.toString() != media_state2->getInstanceId()) {
            media_state2->setDisplayedDataKey(selection_context->primarySelectedData().toString());
        }

        REQUIRE(media_state2->displayedDataKey() == "data_from_media1");

        // Media 1 should NOT respond to its own selection
        if (source1.editor_instance_id.toString() != media_state1->getInstanceId()) {
            // This block should not execute
            media_state1->setDisplayedDataKey("should_not_happen");
        }

        // Media 1's state should not have been auto-updated
        REQUIRE(media_state1->displayedDataKey().isEmpty());
    }

    SECTION("SelectionSource correctly identifies originating widget") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        auto dm_state = std::make_shared<DataManagerWidgetState>();

        workspace.registerState(media_state);
        workspace.registerState(dm_state);

        // All instance IDs should be unique
        REQUIRE(media_state->getInstanceId() != dm_state->getInstanceId());

        // Create selection sources
        SelectionSource media_source{EditorInstanceId(media_state->getInstanceId()), "feature_table"};
        SelectionSource dm_source{EditorInstanceId(dm_state->getInstanceId()), "feature_table"};

        // They should be distinguishable
        REQUIRE_FALSE(media_source == dm_source);
        REQUIRE(media_source.editor_instance_id.toString() == media_state->getInstanceId());
        REQUIRE(dm_source.editor_instance_id.toString() == dm_state->getInstanceId());
    }

    SECTION("DataManager_Widget selection propagates to Media_Widget state") {
        // This tests the full signal chain:
        // DataManager_Widget feature table -> DataManagerWidgetState -> SelectionContext -> Media_Widget handler

        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

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
            if (source.editor_instance_id.toString() != media_state->getInstanceId()) {
                media_received_selection = true;
                received_key = selection_context->primarySelectedData().toString();
                media_state->setDisplayedDataKey(received_key);
            }
        });

        // Simulate DataManager_Widget selecting a feature
        dm_state->setSelectedDataKey("whisker_data");

        // This would normally trigger the state -> SelectionContext connection in the widget
        // Here we simulate it:
        SelectionSource dm_source{EditorInstanceId(dm_state->getInstanceId()), "feature_table"};
        selection_context->setSelectedData(SelectedDataKey(dm_state->selectedDataKey()), dm_source);

        // Verify the chain worked
        REQUIRE(media_received_selection);
        REQUIRE(received_key == "whisker_data");
        REQUIRE(media_state->displayedDataKey() == "whisker_data");
    }
}

TEST_CASE("Workspace serialization with multiple widget states", "[EditorState][EditorRegistry][integration]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Workspace with mixed state types serializes correctly") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto media_state = std::make_shared<MediaWidgetState>();
        media_state->setDisplayName("Media Viewer 1");
        media_state->setDisplayedDataKey("video_data");
        workspace.registerState(media_state);

        auto dm_state = std::make_shared<DataManagerWidgetState>();
        dm_state->setDisplayName("Data Manager");
        dm_state->setSelectedDataKey("whisker_lines");
        workspace.registerState(dm_state);

        REQUIRE(workspace.allStates().size() == 2);

        // Note: Full workspace serialization would require the factory system
        // to be set up with state factories. This test verifies states can coexist.
        auto media_json = media_state->toJson();
        auto dm_json = dm_state->toJson();

        REQUIRE_FALSE(media_json.empty());
        REQUIRE_FALSE(dm_json.empty());
    }
}

// === Phase 2.7: DataTransformWidgetState Integration Tests ===
// These tests verify that DataTransform_Widget can operate without an embedded
// Feature_Table_Widget by relying on SelectionContext for input data selection.

TEST_CASE("DataTransformWidgetState integration", "[EditorState][DataTransform][integration]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("DataTransformWidgetState can be registered with EditorRegistry") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto state = std::make_shared<DataTransformWidgetState>();
        workspace.registerState(state);

        REQUIRE(workspace.allStates().size() == 1);
        REQUIRE(workspace.state(EditorInstanceId(state->getInstanceId())) == state);
        REQUIRE(state->getTypeName() == "DataTransformWidget");

        workspace.unregisterState(EditorInstanceId(state->getInstanceId()));
        REQUIRE(workspace.allStates().empty());
    }

    SECTION("DataTransformWidgetState tracks input data key changes") {
        auto state = std::make_shared<DataTransformWidgetState>();
        
        QSignalSpy input_spy(state.get(), &DataTransformWidgetState::selectedInputDataKeyChanged);
        
        state->setSelectedInputDataKey("mask_data");
        
        REQUIRE(input_spy.count() == 1);
        REQUIRE(state->selectedInputDataKey() == "mask_data");
        
        // Setting same value should not emit again
        state->setSelectedInputDataKey("mask_data");
        REQUIRE(input_spy.count() == 1);
    }

    SECTION("DataTransformWidgetState tracks operation selection") {
        auto state = std::make_shared<DataTransformWidgetState>();
        
        QSignalSpy op_spy(state.get(), &DataTransformWidgetState::selectedOperationChanged);
        
        state->setSelectedOperation("Calculate Area");
        
        REQUIRE(op_spy.count() == 1);
        REQUIRE(state->selectedOperation() == "Calculate Area");
    }

    SECTION("DataTransformWidgetState serializes and deserializes correctly") {
        auto state = std::make_shared<DataTransformWidgetState>();
        state->setDisplayName("My Transform");
        state->setSelectedInputDataKey("test_input");
        state->setSelectedOperation("Filter");
        state->setLastOutputName("filtered_output");
        
        std::string json = state->toJson();
        REQUIRE_FALSE(json.empty());
        
        auto restored_state = std::make_shared<DataTransformWidgetState>();
        REQUIRE(restored_state->fromJson(json));
        
        REQUIRE(restored_state->getDisplayName() == "My Transform");
        REQUIRE(restored_state->selectedInputDataKey() == "test_input");
        REQUIRE(restored_state->selectedOperation() == "Filter");
        REQUIRE(restored_state->lastOutputName() == "filtered_output");
        REQUIRE(restored_state->getInstanceId() == state->getInstanceId());
    }

    SECTION("DataTransform responds to SelectionContext from DataManager_Widget") {
        // This is the key Phase 2.7 test: DataTransform_Widget receives input
        // selection entirely from SelectionContext, not from an embedded feature table
        
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto dm_state = std::make_shared<DataManagerWidgetState>();
        auto transform_state = std::make_shared<DataTransformWidgetState>();

        workspace.registerState(dm_state);
        workspace.registerState(transform_state);

        auto * selection_context = workspace.selectionContext();

        // Track selection changes received by DataTransform_Widget using LEGACY signal
        bool transform_received_selection = false;
        QString received_key;

        QObject::connect(selection_context, &SelectionContext::selectionChanged,
                         [&](SelectionSource const & source) {
            // Simulate DataTransform_Widget's _onExternalSelectionChanged behavior (legacy)
            if (source.editor_instance_id.toString() != transform_state->getInstanceId()) {
                transform_received_selection = true;
                received_key = selection_context->primarySelectedData().toString();
                transform_state->setSelectedInputDataKey(received_key);
            }
        });

        // Simulate DataManager_Widget selecting a feature
        dm_state->setSelectedDataKey("analog_signal");
        SelectionSource dm_source{EditorInstanceId(dm_state->getInstanceId()), "feature_table"};
        selection_context->setSelectedData(SelectedDataKey(dm_state->selectedDataKey()), dm_source);

        // Verify the chain worked
        REQUIRE(transform_received_selection);
        REQUIRE(received_key == "analog_signal");
        REQUIRE(transform_state->selectedInputDataKey() == "analog_signal");
    }

    SECTION("DataTransform responds to dataFocusChanged (Phase 4.2 passive awareness)") {
        // Phase 4.2 test: DataTransform_Widget uses dataFocusChanged signal
        // via the DataFocusAware interface pattern
        
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto dm_state = std::make_shared<DataManagerWidgetState>();
        auto transform_state = std::make_shared<DataTransformWidgetState>();

        workspace.registerState(dm_state);
        workspace.registerState(transform_state);

        auto * selection_context = workspace.selectionContext();

        // Track dataFocusChanged signal (the NEW pattern from Phase 4.2)
        bool transform_received_focus = false;
        QString received_key;
        QString received_type;

        QObject::connect(selection_context, &SelectionContext::dataFocusChanged,
                         [&](SelectedDataKey const & data_key,
                             QString const & data_type,
                             SelectionSource const & source) {
            // Simulate DataTransform_Widget's onDataFocusChanged behavior
            if (source.editor_instance_id.toString() != transform_state->getInstanceId()) {
                transform_received_focus = true;
                received_key = data_key.toString();
                received_type = data_type;
                transform_state->setSelectedInputDataKey(data_key.toString());
            }
        });

        // Simulate DataManager_Widget selecting a feature using setSelectedData
        // This should now also emit dataFocusChanged for passive awareness
        dm_state->setSelectedDataKey("line_data");
        SelectionSource dm_source{EditorInstanceId(dm_state->getInstanceId()), "feature_table"};
        selection_context->setSelectedData(SelectedDataKey(dm_state->selectedDataKey()), dm_source);

        // Verify the dataFocusChanged signal was emitted and received
        REQUIRE(transform_received_focus);
        REQUIRE(received_key == "line_data");
        REQUIRE(transform_state->selectedInputDataKey() == "line_data");
    }

    SECTION("setDataFocus emits dataFocusChanged with type information") {
        // Test the explicit setDataFocus API which includes type information
        
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto transform_state = std::make_shared<DataTransformWidgetState>();
        workspace.registerState(transform_state);

        auto * selection_context = workspace.selectionContext();

        // Track dataFocusChanged signal
        bool received_signal = false;
        QString received_key;
        QString received_type;

        QObject::connect(selection_context, &SelectionContext::dataFocusChanged,
                         [&](SelectedDataKey const & data_key,
                             QString const & data_type,
                             SelectionSource const & /* source */) {
            received_signal = true;
            received_key = data_key.toString();
            received_type = data_type;
        });

        // Use explicit setDataFocus with type information
        SelectionSource source{EditorInstanceId("external_widget"), "feature_table"};
        selection_context->setDataFocus(SelectedDataKey("mask_data"), "MaskData", source);

        REQUIRE(received_signal);
        REQUIRE(received_key == "mask_data");
        REQUIRE(received_type == "MaskData");
        REQUIRE(selection_context->dataFocus().toString() == "mask_data");
        REQUIRE(selection_context->dataFocusType() == "MaskData");
    }

    SECTION("DataTransform ignores own selections (no circular updates)") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto transform_state = std::make_shared<DataTransformWidgetState>();
        workspace.registerState(transform_state);

        auto * selection_context = workspace.selectionContext();

        // Set initial state
        transform_state->setSelectedInputDataKey("initial_key");

        // Simulate selection originating from DataTransform itself
        SelectionSource own_source{EditorInstanceId(transform_state->getInstanceId()), "internal"};
        selection_context->setSelectedData(SelectedDataKey("new_key"), own_source);

        // Handler should filter out own selections
        if (own_source.editor_instance_id.toString() != transform_state->getInstanceId()) {
            transform_state->setSelectedInputDataKey(selection_context->primarySelectedData().toString());
        }

        // State should remain unchanged
        REQUIRE(transform_state->selectedInputDataKey() == "initial_key");
    }

    SECTION("Multiple widget types coexist with DataTransformWidgetState") {
        auto dm = std::make_shared<DataManager>();
        EditorRegistry workspace(dm);

        auto dm_state = std::make_shared<DataManagerWidgetState>();
        auto media_state = std::make_shared<MediaWidgetState>();
        auto transform_state = std::make_shared<DataTransformWidgetState>();

        workspace.registerState(dm_state);
        workspace.registerState(media_state);
        workspace.registerState(transform_state);

        REQUIRE(workspace.allStates().size() == 3);

        // All have unique IDs
        REQUIRE(dm_state->getInstanceId() != media_state->getInstanceId());
        REQUIRE(dm_state->getInstanceId() != transform_state->getInstanceId());
        REQUIRE(media_state->getInstanceId() != transform_state->getInstanceId());

        // All have correct type names
        REQUIRE(dm_state->getTypeName() == "DataManagerWidget");
        REQUIRE(media_state->getTypeName() == "MediaWidget");
        REQUIRE(transform_state->getTypeName() == "DataTransformWidget");

        auto * selection_context = workspace.selectionContext();

        // Track dataFocusChanged for passive awareness widgets
        bool media_received = false;
        bool transform_received = false;

        QObject::connect(selection_context, &SelectionContext::dataFocusChanged,
                         [&](SelectedDataKey const & data_key,
                             QString const & /* data_type */,
                             SelectionSource const & source) {
            // Simulate Media and Transform responding via dataFocusChanged
            if (source.editor_instance_id.toString() != media_state->getInstanceId()) {
                media_received = true;
                media_state->setDisplayedDataKey(data_key.toString());
            }
            if (source.editor_instance_id.toString() != transform_state->getInstanceId()) {
                transform_received = true;
                transform_state->setSelectedInputDataKey(data_key.toString());
            }
        });

        // DataManager selects -> both Media and Transform should respond via dataFocusChanged
        SelectionSource dm_source{EditorInstanceId(dm_state->getInstanceId()), "feature_table"};
        selection_context->setSelectedData(SelectedDataKey("shared_data"), dm_source);

        REQUIRE(media_received);
        REQUIRE(transform_received);
        REQUIRE(media_state->displayedDataKey() == "shared_data");
        REQUIRE(transform_state->selectedInputDataKey() == "shared_data");
    }
}
