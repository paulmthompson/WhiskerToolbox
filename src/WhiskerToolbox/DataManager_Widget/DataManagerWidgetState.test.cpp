#include "DataManagerWidgetState.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/WorkspaceManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include <memory>
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

// === Integration Tests: DataManagerWidgetState + SelectionContext ===

TEST_CASE("DataManagerWidgetState integrates with SelectionContext", "[DataManagerWidgetState][SelectionContext][Integration]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("State selection change updates SelectionContext via signal connection") {
        // Setup: Create state and selection context
        auto state = std::make_shared<DataManagerWidgetState>();
        SelectionContext selection_context;

        // Connect state to selection context (mirroring what DataManager_Widget does)
        QObject::connect(state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        &selection_context, [&state, &selection_context](QString const & key) {
            SelectionSource source{state->getInstanceId(), QStringLiteral("feature_table")};
            selection_context.setSelectedData(key, source);
        });

        // Initially nothing is selected
        REQUIRE(selection_context.primarySelectedData().isEmpty());

        // When state's selected key changes, SelectionContext should update
        state->setSelectedDataKey("whisker_data");

        REQUIRE(selection_context.primarySelectedData() == "whisker_data");
        REQUIRE(selection_context.isSelected("whisker_data"));
    }

    SECTION("Multiple state changes propagate correctly to SelectionContext") {
        auto state = std::make_shared<DataManagerWidgetState>();
        SelectionContext selection_context;

        QObject::connect(state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        &selection_context, [&state, &selection_context](QString const & key) {
            SelectionSource source{state->getInstanceId(), QStringLiteral("feature_table")};
            selection_context.setSelectedData(key, source);
        });

        // Select first item
        state->setSelectedDataKey("data_1");
        REQUIRE(selection_context.primarySelectedData() == "data_1");

        // Select second item (should replace, not add)
        state->setSelectedDataKey("data_2");
        REQUIRE(selection_context.primarySelectedData() == "data_2");
        REQUIRE_FALSE(selection_context.isSelected("data_1"));
        REQUIRE(selection_context.allSelectedData().size() == 1);

        // Select third item
        state->setSelectedDataKey("data_3");
        REQUIRE(selection_context.primarySelectedData() == "data_3");
    }

    SECTION("SelectionContext emits signal when state updates it") {
        auto state = std::make_shared<DataManagerWidgetState>();
        SelectionContext selection_context;
        QSignalSpy spy(&selection_context, &SelectionContext::selectionChanged);

        QObject::connect(state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        &selection_context, [&state, &selection_context](QString const & key) {
            SelectionSource source{state->getInstanceId(), QStringLiteral("feature_table")};
            selection_context.setSelectedData(key, source);
        });

        state->setSelectedDataKey("test_key");

        REQUIRE(spy.count() == 1);

        // Verify the selection source is correct
        auto source = spy.at(0).at(0).value<SelectionSource>();
        REQUIRE(source.editor_instance_id == state->getInstanceId());
        REQUIRE(source.widget_id == "feature_table");
    }

    SECTION("Clearing selection in state clears SelectionContext") {
        auto state = std::make_shared<DataManagerWidgetState>();
        SelectionContext selection_context;

        QObject::connect(state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        &selection_context, [&state, &selection_context](QString const & key) {
            if (key.isEmpty()) {
                SelectionSource source{state->getInstanceId(), QStringLiteral("feature_table")};
                selection_context.clearSelection(source);
            } else {
                SelectionSource source{state->getInstanceId(), QStringLiteral("feature_table")};
                selection_context.setSelectedData(key, source);
            }
        });

        // Select something
        state->setSelectedDataKey("some_data");
        REQUIRE(selection_context.isSelected("some_data"));

        // Clear by setting empty key
        state->setSelectedDataKey("");
        REQUIRE(selection_context.primarySelectedData().isEmpty());
        REQUIRE(selection_context.allSelectedData().empty());
    }
}

TEST_CASE("DataManagerWidgetState works with WorkspaceManager", "[DataManagerWidgetState][WorkspaceManager][Integration]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("State registered with WorkspaceManager can update shared SelectionContext") {
        // Create workspace manager (similar to how MainWindow does it)
        WorkspaceManager workspace_manager(nullptr);

        // Create and register state
        auto state = std::make_shared<DataManagerWidgetState>();
        workspace_manager.registerState(state);

        // Get the shared selection context
        SelectionContext * selection_context = workspace_manager.selectionContext();
        REQUIRE(selection_context != nullptr);

        // Connect state to selection context
        QObject::connect(state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        selection_context, [&state, selection_context](QString const & key) {
            SelectionSource source{state->getInstanceId(), QStringLiteral("feature_table")};
            selection_context->setSelectedData(key, source);
        });

        // Update state
        state->setSelectedDataKey("workspace_data");

        // Verify SelectionContext was updated
        REQUIRE(selection_context->primarySelectedData() == "workspace_data");
    }

    SECTION("Multiple states can share the same SelectionContext") {
        WorkspaceManager workspace_manager(nullptr);

        auto state1 = std::make_shared<DataManagerWidgetState>();
        auto state2 = std::make_shared<DataManagerWidgetState>();

        workspace_manager.registerState(state1);
        workspace_manager.registerState(state2);

        SelectionContext * selection_context = workspace_manager.selectionContext();

        // Connect both states
        QObject::connect(state1.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        selection_context, [&state1, selection_context](QString const & key) {
            SelectionSource source{state1->getInstanceId(), QStringLiteral("feature_table")};
            selection_context->setSelectedData(key, source);
        });

        QObject::connect(state2.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        selection_context, [&state2, selection_context](QString const & key) {
            SelectionSource source{state2->getInstanceId(), QStringLiteral("feature_table")};
            selection_context->setSelectedData(key, source);
        });

        // State1 selects
        state1->setSelectedDataKey("from_state1");
        REQUIRE(selection_context->primarySelectedData() == "from_state1");

        // State2 selects (should override)
        state2->setSelectedDataKey("from_state2");
        REQUIRE(selection_context->primarySelectedData() == "from_state2");

        // State1 selects again
        state1->setSelectedDataKey("back_to_state1");
        REQUIRE(selection_context->primarySelectedData() == "back_to_state1");
    }

    SECTION("SelectionSource correctly identifies which state made selection") {
        WorkspaceManager workspace_manager(nullptr);

        auto state1 = std::make_shared<DataManagerWidgetState>();
        auto state2 = std::make_shared<DataManagerWidgetState>();

        workspace_manager.registerState(state1);
        workspace_manager.registerState(state2);

        SelectionContext * selection_context = workspace_manager.selectionContext();
        QSignalSpy spy(selection_context, &SelectionContext::selectionChanged);

        QObject::connect(state1.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        selection_context, [&state1, selection_context](QString const & key) {
            SelectionSource source{state1->getInstanceId(), QStringLiteral("feature_table")};
            selection_context->setSelectedData(key, source);
        });

        QObject::connect(state2.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                        selection_context, [&state2, selection_context](QString const & key) {
            SelectionSource source{state2->getInstanceId(), QStringLiteral("feature_table")};
            selection_context->setSelectedData(key, source);
        });

        // State1 makes selection
        state1->setSelectedDataKey("data1");
        REQUIRE(spy.count() == 1);
        auto source1 = spy.at(0).at(0).value<SelectionSource>();
        REQUIRE(source1.editor_instance_id == state1->getInstanceId());

        // State2 makes selection
        state2->setSelectedDataKey("data2");
        REQUIRE(spy.count() == 2);
        auto source2 = spy.at(1).at(0).value<SelectionSource>();
        REQUIRE(source2.editor_instance_id == state2->getInstanceId());

        // Verify the sources are different
        REQUIRE(source1.editor_instance_id != source2.editor_instance_id);
    }
}
