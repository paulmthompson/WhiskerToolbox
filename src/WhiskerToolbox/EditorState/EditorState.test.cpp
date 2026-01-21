#include <catch2/catch_test_macros.hpp>

#include "EditorState/EditorState.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/EditorRegistry.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include <memory>

// === Test EditorState Implementation ===

/**
 * @brief Simple test state for unit testing
 */
struct TestStateData {
    std::string name = "default";
    int value = 0;
    bool enabled = false;
};

class TestState : public EditorState {
    Q_OBJECT

public:
    explicit TestState(QObject * parent = nullptr)
        : EditorState(parent) {
    }

    [[nodiscard]] QString getTypeName() const override { return "TestState"; }

    [[nodiscard]] std::string toJson() const override { return rfl::json::write(_data); }

    bool fromJson(std::string const & json) override {
        auto result = rfl::json::read<TestStateData>(json);
        if (result) {
            _data = *result;
            emit stateChanged();
            return true;
        }
        return false;
    }

    // Typed accessors
    void setName(std::string const & name) {
        if (_data.name != name) {
            _data.name = name;
            markDirty();
            emit nameChanged(QString::fromStdString(name));
        }
    }

    [[nodiscard]] std::string getName() const { return _data.name; }

    void setValue(int value) {
        if (_data.value != value) {
            _data.value = value;
            markDirty();
            emit valueChanged(value);
        }
    }

    [[nodiscard]] int getValue() const { return _data.value; }

    void setEnabled(bool enabled) {
        if (_data.enabled != enabled) {
            _data.enabled = enabled;
            markDirty();
            emit enabledChanged(enabled);
        }
    }

    [[nodiscard]] bool isEnabled() const { return _data.enabled; }

signals:
    void nameChanged(QString const & name);
    void valueChanged(int value);
    void enabledChanged(bool enabled);

private:
    TestStateData _data;
};

// Include moc for test state
#include "EditorState.test.moc"

// === EditorState Tests ===

TEST_CASE("EditorState basics", "[EditorState]") {
    // Need QCoreApplication for Qt signals
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Instance ID is unique") {
        TestState state1;
        TestState state2;

        REQUIRE_FALSE(state1.getInstanceId().isEmpty());
        REQUIRE_FALSE(state2.getInstanceId().isEmpty());
        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }

    SECTION("Type name is correct") {
        TestState state;
        REQUIRE(state.getTypeName() == "TestState");
    }

    SECTION("Display name defaults and can be set") {
        TestState state;
        REQUIRE(state.getDisplayName() == "Untitled");

        state.setDisplayName("My Test State");
        REQUIRE(state.getDisplayName() == "My Test State");
    }

    SECTION("Dirty state tracking") {
        TestState state;
        REQUIRE_FALSE(state.isDirty());

        state.setValue(42);
        REQUIRE(state.isDirty());

        state.markClean();
        REQUIRE_FALSE(state.isDirty());
    }
}

TEST_CASE("EditorState serialization", "[EditorState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Round-trip serialization") {
        TestState original;
        original.setName("test_name");
        original.setValue(123);
        original.setEnabled(true);

        auto json = original.toJson();

        TestState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getName() == "test_name");
        REQUIRE(restored.getValue() == 123);
        REQUIRE(restored.isEnabled() == true);
    }

    SECTION("Invalid JSON returns false") {
        TestState state;
        REQUIRE_FALSE(state.fromJson("not valid json"));
        REQUIRE_FALSE(state.fromJson("{\"invalid\": \"schema\"}"));
    }
}

TEST_CASE("EditorState signals", "[EditorState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("stateChanged emitted on modification") {
        TestState state;
        QSignalSpy spy(&state, &EditorState::stateChanged);

        state.setValue(42);
        REQUIRE(spy.count() == 1);

        state.setName("new_name");
        REQUIRE(spy.count() == 2);
    }

    SECTION("dirtyChanged emitted appropriately") {
        TestState state;
        QSignalSpy spy(&state, &EditorState::dirtyChanged);

        state.setValue(42);// First modification
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toBool() == true);

        state.setValue(43);// Already dirty, no signal
        REQUIRE(spy.count() == 1);

        state.markClean();
        REQUIRE(spy.count() == 2);
        REQUIRE(spy.at(1).at(0).toBool() == false);
    }

    SECTION("displayNameChanged emitted") {
        TestState state;
        QSignalSpy spy(&state, &EditorState::displayNameChanged);

        state.setDisplayName("New Name");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "New Name");
    }
}

// === SelectionContext Tests ===

TEST_CASE("SelectionContext data selection", "[SelectionContext]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SelectionContext ctx;
    SelectionSource source{"editor1", "table"};

    SECTION("Initial state is empty") {
        REQUIRE(ctx.primarySelectedData().isEmpty());
        REQUIRE(ctx.allSelectedData().empty());
    }

    SECTION("Single selection") {
        ctx.setSelectedData("data1", source);

        REQUIRE(ctx.primarySelectedData() == "data1");
        REQUIRE(ctx.allSelectedData().size() == 1);
        REQUIRE(ctx.isSelected("data1"));
        REQUIRE_FALSE(ctx.isSelected("data2"));
    }

    SECTION("Multi-selection") {
        ctx.setSelectedData("data1", source);
        ctx.addToSelection("data2", source);
        ctx.addToSelection("data3", source);

        REQUIRE(ctx.primarySelectedData() == "data1");
        REQUIRE(ctx.allSelectedData().size() == 3);
        REQUIRE(ctx.isSelected("data1"));
        REQUIRE(ctx.isSelected("data2"));
        REQUIRE(ctx.isSelected("data3"));
    }

    SECTION("Remove from selection") {
        ctx.setSelectedData("data1", source);
        ctx.addToSelection("data2", source);

        ctx.removeFromSelection("data1", source);

        REQUIRE(ctx.primarySelectedData() == "data2");
        REQUIRE(ctx.allSelectedData().size() == 1);
        REQUIRE_FALSE(ctx.isSelected("data1"));
    }

    SECTION("Clear selection") {
        ctx.setSelectedData("data1", source);
        ctx.addToSelection("data2", source);

        ctx.clearSelection(source);

        REQUIRE(ctx.primarySelectedData().isEmpty());
        REQUIRE(ctx.allSelectedData().empty());
    }

    SECTION("Setting selection clears previous") {
        ctx.setSelectedData("data1", source);
        ctx.addToSelection("data2", source);

        ctx.setSelectedData("data3", source);

        REQUIRE(ctx.primarySelectedData() == "data3");
        REQUIRE(ctx.allSelectedData().size() == 1);
    }
}

TEST_CASE("SelectionContext entity selection", "[SelectionContext]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SelectionContext ctx;
    SelectionSource source{"editor1", "canvas"};

    SECTION("Entity selection") {
        ctx.setSelectedEntities({1, 2, 3}, source);

        auto entities = ctx.selectedEntities();
        REQUIRE(entities.size() == 3);
        REQUIRE(ctx.isEntitySelected(1));
        REQUIRE(ctx.isEntitySelected(2));
        REQUIRE(ctx.isEntitySelected(3));
        REQUIRE_FALSE(ctx.isEntitySelected(4));
    }

    SECTION("Add entities") {
        ctx.setSelectedEntities({1, 2}, source);
        ctx.addSelectedEntities({3, 4}, source);

        REQUIRE(ctx.selectedEntities().size() == 4);
    }

    SECTION("Clear entity selection") {
        ctx.setSelectedEntities({1, 2, 3}, source);
        ctx.clearEntitySelection(source);

        REQUIRE(ctx.selectedEntities().empty());
    }

    SECTION("Data selection clears entity selection") {
        ctx.setSelectedEntities({1, 2, 3}, source);
        ctx.setSelectedData("data1", source);

        REQUIRE(ctx.selectedEntities().empty());
    }
}

TEST_CASE("SelectionContext active editor", "[SelectionContext]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SelectionContext ctx;

    SECTION("Initial active editor is empty") {
        REQUIRE(ctx.activeEditorId().isEmpty());
    }

    SECTION("Set active editor") {
        ctx.setActiveEditor("editor1");
        REQUIRE(ctx.activeEditorId() == "editor1");
    }

    SECTION("activeEditorChanged signal") {
        QSignalSpy spy(&ctx, &SelectionContext::activeEditorChanged);

        ctx.setActiveEditor("editor1");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "editor1");

        // Same editor doesn't emit
        ctx.setActiveEditor("editor1");
        REQUIRE(spy.count() == 1);
    }
}

TEST_CASE("SelectionContext signals", "[SelectionContext]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SelectionContext ctx;
    SelectionSource source{"editor1", "table"};

    SECTION("selectionChanged emitted") {
        QSignalSpy spy(&ctx, &SelectionContext::selectionChanged);

        ctx.setSelectedData("data1", source);
        REQUIRE(spy.count() == 1);

        ctx.addToSelection("data2", source);
        REQUIRE(spy.count() == 2);

        ctx.removeFromSelection("data1", source);
        REQUIRE(spy.count() == 3);

        ctx.clearSelection(source);
        REQUIRE(spy.count() == 4);
    }

    SECTION("entitySelectionChanged emitted") {
        QSignalSpy spy(&ctx, &SelectionContext::entitySelectionChanged);

        ctx.setSelectedEntities({1, 2}, source);
        REQUIRE(spy.count() == 1);

        ctx.addSelectedEntities({3}, source);
        REQUIRE(spy.count() == 2);

        ctx.clearEntitySelection(source);
        REQUIRE(spy.count() == 3);
    }
}

TEST_CASE("SelectionContext properties context", "[SelectionContext]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SelectionContext ctx;
    SelectionSource source{"editor1", "table"};

    SECTION("propertiesContext updated on interaction") {
        ctx.notifyInteraction("editor1");
        ctx.setSelectedData("data1", source);
        ctx.setSelectedDataType("LineData");

        auto props = ctx.propertiesContext();
        REQUIRE(props.last_interacted_editor == "editor1");
        REQUIRE(props.selected_data_key == "data1");
        REQUIRE(props.data_type == "LineData");
    }

    SECTION("propertiesContextChanged signal") {
        QSignalSpy spy(&ctx, &SelectionContext::propertiesContextChanged);

        ctx.notifyInteraction("editor1");
        REQUIRE(spy.count() == 1);

        ctx.setSelectedData("data1", source);
        REQUIRE(spy.count() == 2);

        ctx.setSelectedDataType("LineData");
        REQUIRE(spy.count() == 3);
    }
}

// === EditorRegistry Tests ===

TEST_CASE("EditorRegistry state registration", "[EditorRegistry]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    EditorRegistry mgr(nullptr);

    SECTION("Register and retrieve state") {
        auto state = std::make_shared<TestState>();
        mgr.registerState(state);

        REQUIRE(mgr.stateCount() == 1);
        REQUIRE(mgr.state(state->getInstanceId()) == state);
    }

    SECTION("Get states by type") {
        auto state1 = std::make_shared<TestState>();
        auto state2 = std::make_shared<TestState>();
        mgr.registerState(state1);
        mgr.registerState(state2);

        auto states = mgr.statesByType("TestState");
        REQUIRE(states.size() == 2);
    }

    SECTION("Unregister state") {
        auto state = std::make_shared<TestState>();
        mgr.registerState(state);

        mgr.unregisterState(state->getInstanceId());
        REQUIRE(mgr.stateCount() == 0);
        REQUIRE(mgr.state(state->getInstanceId()) == nullptr);
    }

    SECTION("stateRegistered signal") {
        QSignalSpy spy(&mgr, &EditorRegistry::stateRegistered);

        auto state = std::make_shared<TestState>();
        mgr.registerState(state);

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == state->getInstanceId());
        REQUIRE(spy.at(0).at(1).toString() == "TestState");
    }
}

TEST_CASE("EditorRegistry editor type factory", "[EditorRegistry]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    EditorRegistry mgr(nullptr);

    SECTION("Register and create via factory") {
        EditorRegistry::EditorTypeInfo info{
            .type_id = "TestState",
            .display_name = "Test State",
            .icon_path = "",
            .menu_path = "",
            .default_zone = "main",
            .allow_multiple = true,
            .create_state = []() { return std::make_shared<TestState>(); },
            .create_view = nullptr,
            .create_properties = nullptr
        };
        mgr.registerType(info);

        REQUIRE(mgr.hasType("TestState"));

        auto state = mgr.createState("TestState");
        REQUIRE(state != nullptr);
        REQUIRE(state->getTypeName() == "TestState");
        
        // createState is a factory method - must explicitly register
        mgr.registerState(state);
        REQUIRE(mgr.stateCount() == 1);
    }

    SECTION("Unknown type returns nullptr") {
        auto state = mgr.createState("UnknownType");
        REQUIRE(state == nullptr);
    }

    SECTION("allTypes returns info") {
        EditorRegistry::EditorTypeInfo info{
            .type_id = "TestState",
            .display_name = "Test State",
            .icon_path = ":/icon.png",
            .menu_path = "",
            .default_zone = "main",
            .allow_multiple = true,
            .create_state = []() { return std::make_shared<TestState>(); },
            .create_view = nullptr,
            .create_properties = nullptr
        };
        mgr.registerType(info);

        auto types = mgr.allTypes();
        REQUIRE(types.size() == 1);
        REQUIRE(types[0].type_id == "TestState");
    }
}

TEST_CASE("EditorRegistry selection context access", "[EditorRegistry]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    EditorRegistry mgr(nullptr);

    SECTION("selectionContext is not null") {
        REQUIRE(mgr.selectionContext() != nullptr);
    }

    SECTION("selectionContext is shared") {
        auto* ctx1 = mgr.selectionContext();
        auto* ctx2 = mgr.selectionContext();
        REQUIRE(ctx1 == ctx2);
    }
}

TEST_CASE("EditorRegistry dirty state tracking", "[EditorRegistry]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    EditorRegistry mgr(nullptr);

    SECTION("hasUnsavedChanges reflects state") {
        auto state = std::make_shared<TestState>();
        mgr.registerState(state);

        REQUIRE_FALSE(mgr.hasUnsavedChanges());

        state->setValue(42);
        REQUIRE(mgr.hasUnsavedChanges());

        mgr.markAllClean();
        REQUIRE_FALSE(mgr.hasUnsavedChanges());
    }

    SECTION("unsavedChangesChanged signal") {
        auto state = std::make_shared<TestState>();
        mgr.registerState(state);

        QSignalSpy spy(&mgr, &EditorRegistry::unsavedChangesChanged);

        state->setValue(42);
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toBool() == true);
    }
}

TEST_CASE("EditorRegistry serialization", "[EditorRegistry]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Round-trip serialization") {
        // Create workspace with states
        EditorRegistry original(nullptr);
        EditorRegistry::EditorTypeInfo info{
            .type_id = "TestState",
            .display_name = "Test State",
            .icon_path = "",
            .menu_path = "",
            .default_zone = "main",
            .allow_multiple = true,
            .create_state = []() { return std::make_shared<TestState>(); },
            .create_view = nullptr,
            .create_properties = nullptr
        };
        original.registerType(info);

        auto state1 = std::dynamic_pointer_cast<TestState>(original.createState("TestState"));
        original.registerState(state1);  // Explicitly register
        state1->setDisplayName("State 1");
        state1->setName("first");
        state1->setValue(100);

        auto state2 = std::dynamic_pointer_cast<TestState>(original.createState("TestState"));
        original.registerState(state2);  // Explicitly register
        state2->setDisplayName("State 2");
        state2->setName("second");
        state2->setValue(200);

        // Set selection
        SelectionSource source{"test", "test"};
        original.selectionContext()->setSelectedData("data1", source);

        // Serialize
        auto json = original.toJson();
        REQUIRE_FALSE(json.empty());

        // Restore to new workspace
        EditorRegistry restored(nullptr);
        restored.registerType(info);

        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.stateCount() == 2);

        // Verify selection was restored
        REQUIRE(restored.selectionContext()->primarySelectedData() == "data1");
    }
}
