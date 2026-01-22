/**
 * @file EditorRegistry.test.cpp
 * @brief Comprehensive unit tests for EditorRegistry
 */

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/StrongTypes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <QApplication>
#include <QLabel>
#include <QSignalSpy>

#include <memory>

// === Mock EditorState for Testing ===

struct MockStateData {
    std::string name = "default";
    int value = 0;
    bool enabled = false;
};

class MockState : public EditorState {
    Q_OBJECT

public:
    explicit MockState(QString type_name = "MockState", QObject * parent = nullptr)
        : EditorState(parent)
        , _type_name(std::move(type_name)) {}

    [[nodiscard]] QString getTypeName() const override { return _type_name; }

    [[nodiscard]] std::string toJson() const override { return rfl::json::write(_data); }

    bool fromJson(std::string const & json) override {
        auto result = rfl::json::read<MockStateData>(json);
        if (result) {
            _data = *result;
            emit stateChanged();
            return true;
        }
        return false;
    }

    void setName(std::string const & name) {
        if (_data.name != name) {
            _data.name = name;
            markDirty();
        }
    }
    [[nodiscard]] std::string getName() const { return _data.name; }

    void setValue(int value) {
        if (_data.value != value) {
            _data.value = value;
            markDirty();
        }
    }
    [[nodiscard]] int getValue() const { return _data.value; }

private:
    QString _type_name;
    MockStateData _data;
};

#include "EditorRegistry.test.moc"

// === Test Fixture ===

class RegistryTestFixture {
public:
    RegistryTestFixture() {
        if (!QApplication::instance()) {
            int argc = 0;
            _app = std::make_unique<QApplication>(argc, nullptr);
        }
        _registry = std::make_unique<EditorRegistry>(nullptr);
    }

protected:
    std::unique_ptr<QApplication> _app;
    std::unique_ptr<EditorRegistry> _registry;

    void registerMockType(QString type_id = "MockState") {
        _registry->registerType({
            .type_id = type_id,
            .display_name = "Mock Editor",
            .default_zone = "main",
            .create_state = [type_id]() { return std::make_shared<MockState>(type_id); },
            .create_view = [](auto) { return new QLabel("View"); },
            .create_properties = nullptr});
    }

    void registerMockTypeWithProperties(QString type_id = "MockWithProps") {
        _registry->registerType({
            .type_id = type_id,
            .display_name = "Mock With Properties",
            .default_zone = "main",
            .create_state = [type_id]() { return std::make_shared<MockState>(type_id); },
            .create_view = [](auto s) { return new QLabel("View:" + s->getInstanceId()); },
            .create_properties = [](auto s) { return new QLabel("Props:" + s->getInstanceId()); }});
    }
};

// === Type Registration Tests ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry type registration",
                 "[EditorRegistry][registration]") {

    SECTION("can register type") {
        bool result = _registry->registerType({
            .type_id = "TestType",
            .display_name = "Test",
            .create_state = []() { return std::make_shared<MockState>(); },
            .create_view = [](auto) { return new QLabel(); }});

        REQUIRE(result == true);
        REQUIRE(_registry->hasType(EditorTypeId("TestType")) == true);
    }

    SECTION("cannot register with empty type_id") {
        bool result = _registry->registerType({
            .type_id = "",
            .create_state = []() { return std::make_shared<MockState>(); },
            .create_view = [](auto) { return new QLabel(); }});

        REQUIRE(result == false);
    }

    SECTION("cannot register same type twice") {
        registerMockType("Duplicate");
        bool result = _registry->registerType({
            .type_id = "Duplicate",
            .create_state = []() { return std::make_shared<MockState>(); },
            .create_view = [](auto) { return new QLabel(); }});

        REQUIRE(result == false);
    }

    SECTION("cannot register with null create_state") {
        bool result = _registry->registerType({
            .type_id = "NullState",
            .create_state = nullptr,
            .create_view = [](auto) { return new QLabel(); }});

        REQUIRE(result == false);
    }

    SECTION("can register with null create_view (state-only types)") {
        bool result = _registry->registerType({
            .type_id = "NullView",
            .create_state = []() { return std::make_shared<MockState>(); },
            .create_view = nullptr});

        // State-only types are allowed (for testing and state-only workflows)
        REQUIRE(result == true);
    }

    SECTION("can register without create_properties") {
        bool result = _registry->registerType({
            .type_id = "NoProps",
            .create_state = []() { return std::make_shared<MockState>(); },
            .create_view = [](auto) { return new QLabel(); },
            .create_properties = nullptr});

        REQUIRE(result == true);
    }

    SECTION("emits typeRegistered signal") {
        QSignalSpy spy(_registry.get(), &EditorRegistry::typeRegistered);
        registerMockType("SignalTest");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).value<EditorTypeId>().toString() == "SignalTest");
    }

    SECTION("can unregister type") {
        registerMockType("ToRemove");
        REQUIRE(_registry->hasType(EditorTypeId("ToRemove")) == true);

        bool result = _registry->unregisterType(EditorTypeId("ToRemove"));

        REQUIRE(result == true);
        REQUIRE(_registry->hasType(EditorTypeId("ToRemove")) == false);
    }

    SECTION("unregister returns false for non-existent type") {
        bool result = _registry->unregisterType(EditorTypeId("NonExistent"));
        REQUIRE(result == false);
    }
}

// === Type Query Tests ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry type queries",
                 "[EditorRegistry][queries]") {

    _registry->registerType({
        .type_id = "Type1",
        .display_name = "Type One",
        .menu_path = "View/Group1",
        .default_zone = "main",
        .create_state = []() { return std::make_shared<MockState>("Type1"); },
        .create_view = [](auto) { return new QLabel(); }});

    _registry->registerType({
        .type_id = "Type2",
        .display_name = "Type Two",
        .menu_path = "View/Group1",
        .default_zone = "right",
        .create_state = []() { return std::make_shared<MockState>("Type2"); },
        .create_view = [](auto) { return new QLabel(); }});

    _registry->registerType({
        .type_id = "Type3",
        .display_name = "Type Three",
        .menu_path = "View/Group2",
        .default_zone = "main",
        .create_state = []() { return std::make_shared<MockState>("Type3"); },
        .create_view = [](auto) { return new QLabel(); }});

    SECTION("typeInfo returns correct info") {
        auto info = _registry->typeInfo(EditorTypeId("Type1"));
        REQUIRE(info.type_id == "Type1");
        REQUIRE(info.display_name == "Type One");
        REQUIRE(info.menu_path == "View/Group1");
        REQUIRE(info.default_zone == "main");
    }

    SECTION("typeInfo returns empty for unknown") {
        auto info = _registry->typeInfo(EditorTypeId("Unknown"));
        REQUIRE(info.type_id.isEmpty());
    }

    SECTION("allTypes returns all registered") {
        auto types = _registry->allTypes();
        REQUIRE(types.size() == 3);
    }

    SECTION("typesByMenuPath filters correctly") {
        auto group1 = _registry->typesByMenuPath("View/Group1");
        REQUIRE(group1.size() == 2);

        auto group2 = _registry->typesByMenuPath("View/Group2");
        REQUIRE(group2.size() == 1);

        auto empty = _registry->typesByMenuPath("View/NoGroup");
        REQUIRE(empty.empty());
    }
}

// === Editor Creation Tests ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry createEditor",
                 "[EditorRegistry][creation]") {

    registerMockTypeWithProperties("TestEditor");

    SECTION("returns all components") {
        auto inst = _registry->createEditor(EditorTypeId("TestEditor"));

        REQUIRE(inst.state != nullptr);
        REQUIRE(inst.view != nullptr);
        REQUIRE(inst.properties != nullptr);
        REQUIRE(inst.state->getTypeName() == "TestEditor");

        delete inst.view;
        delete inst.properties;
    }

    SECTION("auto-registers state") {
        auto inst = _registry->createEditor(EditorTypeId("TestEditor"));

        REQUIRE(_registry->stateCount() == 1);
        REQUIRE(_registry->state(EditorInstanceId(inst.state->getInstanceId())) == inst.state);

        delete inst.view;
        delete inst.properties;
    }

    SECTION("returns empty for unknown type") {
        auto inst = _registry->createEditor(EditorTypeId("Unknown"));

        REQUIRE(inst.state == nullptr);
        REQUIRE(inst.view == nullptr);
        REQUIRE(inst.properties == nullptr);
    }

    SECTION("emits editorCreated signal") {
        QSignalSpy spy(_registry.get(), &EditorRegistry::editorCreated);
        auto inst = _registry->createEditor(EditorTypeId("TestEditor"));

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).value<EditorInstanceId>().toString() == inst.state->getInstanceId());
        REQUIRE(spy.at(0).at(1).value<EditorTypeId>().toString() == "TestEditor");

        delete inst.view;
        delete inst.properties;
    }

    SECTION("properties is null when no factory") {
        registerMockType("NoPropsType");
        auto inst = _registry->createEditor(EditorTypeId("NoPropsType"));

        REQUIRE(inst.state != nullptr);
        REQUIRE(inst.view != nullptr);
        REQUIRE(inst.properties == nullptr);

        delete inst.view;
    }
}

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry createState",
                 "[EditorRegistry][creation]") {

    registerMockType();

    SECTION("creates state without registering") {
        auto state = _registry->createState(EditorTypeId("MockState"));

        REQUIRE(state != nullptr);
        REQUIRE(_registry->stateCount() == 0);  // Not auto-registered
    }

    SECTION("returns nullptr for unknown type") {
        auto state = _registry->createState(EditorTypeId("Unknown"));
        REQUIRE(state == nullptr);
    }
}

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry createView/createProperties",
                 "[EditorRegistry][creation]") {

    registerMockTypeWithProperties("ViewTest");

    SECTION("createView creates view for state") {
        auto state = std::make_shared<MockState>("ViewTest");
        auto * view = _registry->createView(state);

        REQUIRE(view != nullptr);
        auto * label = qobject_cast<QLabel *>(view);
        REQUIRE(label != nullptr);
        REQUIRE(label->text().contains(state->getInstanceId()));

        delete view;
    }

    SECTION("createProperties creates properties for state") {
        auto state = std::make_shared<MockState>("ViewTest");
        auto * props = _registry->createProperties(state);

        REQUIRE(props != nullptr);
        auto * label = qobject_cast<QLabel *>(props);
        REQUIRE(label != nullptr);
        REQUIRE(label->text().contains(state->getInstanceId()));

        delete props;
    }

    SECTION("createProperties returns null when no factory") {
        registerMockType("NoPropsType");
        auto state = std::make_shared<MockState>("NoPropsType");
        auto * props = _registry->createProperties(state);

        REQUIRE(props == nullptr);
    }

    SECTION("create functions return null for null state") {
        REQUIRE(_registry->createView(nullptr) == nullptr);
        REQUIRE(_registry->createProperties(nullptr) == nullptr);
    }
}

// === State Registry Tests ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry state registry",
                 "[EditorRegistry][state]") {

    SECTION("registerState adds state") {
        auto state = std::make_shared<MockState>();
        _registry->registerState(state);

        REQUIRE(_registry->stateCount() == 1);
        REQUIRE(_registry->state(EditorInstanceId(state->getInstanceId())) == state);
    }

    SECTION("registerState is idempotent") {
        auto state = std::make_shared<MockState>();
        _registry->registerState(state);
        _registry->registerState(state);  // Second call

        REQUIRE(_registry->stateCount() == 1);
    }

    SECTION("registerState ignores null") {
        _registry->registerState(nullptr);
        REQUIRE(_registry->stateCount() == 0);
    }

    SECTION("unregisterState removes state") {
        auto state = std::make_shared<MockState>();
        _registry->registerState(state);

        _registry->unregisterState(EditorInstanceId(state->getInstanceId()));

        REQUIRE(_registry->stateCount() == 0);
        REQUIRE(_registry->state(EditorInstanceId(state->getInstanceId())) == nullptr);
    }

    SECTION("statesByType filters correctly") {
        registerMockType("TypeA");
        registerMockType("TypeB");

        _registry->createEditor(EditorTypeId("TypeA"));
        _registry->createEditor(EditorTypeId("TypeA"));
        _registry->createEditor(EditorTypeId("TypeB"));

        auto typeA = _registry->statesByType(EditorTypeId("TypeA"));
        REQUIRE(typeA.size() == 2);

        auto typeB = _registry->statesByType(EditorTypeId("TypeB"));
        REQUIRE(typeB.size() == 1);
    }

    SECTION("allStates returns all") {
        registerMockType();
        _registry->createEditor(EditorTypeId("MockState"));
        _registry->createEditor(EditorTypeId("MockState"));

        auto all = _registry->allStates();
        REQUIRE(all.size() == 2);
    }

    SECTION("emits stateRegistered signal") {
        QSignalSpy spy(_registry.get(), &EditorRegistry::stateRegistered);

        auto state = std::make_shared<MockState>("TestType");
        _registry->registerState(state);

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).value<EditorInstanceId>().toString() == state->getInstanceId());
        REQUIRE(spy.at(0).at(1).value<EditorTypeId>().toString() == "TestType");
    }

    SECTION("emits stateUnregistered signal") {
        auto state = std::make_shared<MockState>();
        _registry->registerState(state);

        QSignalSpy spy(_registry.get(), &EditorRegistry::stateUnregistered);
        _registry->unregisterState(EditorInstanceId(state->getInstanceId()));

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).value<EditorInstanceId>().toString() == state->getInstanceId());
    }
}

// === Selection Context Tests ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry selection context",
                 "[EditorRegistry][selection]") {

    SECTION("selectionContext is not null") {
        REQUIRE(_registry->selectionContext() != nullptr);
    }

    SECTION("selectionContext is consistent") {
        auto * ctx1 = _registry->selectionContext();
        auto * ctx2 = _registry->selectionContext();
        REQUIRE(ctx1 == ctx2);
    }
}

// === Dirty State Tests ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry dirty tracking",
                 "[EditorRegistry][dirty]") {

    registerMockType();

    SECTION("hasUnsavedChanges reflects state") {
        auto inst = _registry->createEditor(EditorTypeId("MockState"));
        auto mock = std::dynamic_pointer_cast<MockState>(inst.state);

        REQUIRE(_registry->hasUnsavedChanges() == false);

        mock->setValue(42);
        REQUIRE(_registry->hasUnsavedChanges() == true);

        _registry->markAllClean();
        REQUIRE(_registry->hasUnsavedChanges() == false);

        delete inst.view;
    }

    SECTION("emits unsavedChangesChanged") {
        auto inst = _registry->createEditor(EditorTypeId("MockState"));
        auto mock = std::dynamic_pointer_cast<MockState>(inst.state);

        QSignalSpy spy(_registry.get(), &EditorRegistry::unsavedChangesChanged);

        mock->setValue(42);
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toBool() == true);

        delete inst.view;
    }
}

// === Serialization Tests ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry serialization",
                 "[EditorRegistry][serialization]") {

    registerMockType();

    SECTION("toJson produces valid JSON") {
        auto inst = _registry->createEditor(EditorTypeId("MockState"));
        auto json = _registry->toJson();

        REQUIRE_FALSE(json.empty());
        REQUIRE(json.find("version") != std::string::npos);
        REQUIRE(json.find("states") != std::string::npos);

        delete inst.view;
    }

    SECTION("round-trip serialization") {
        // Create and configure state
        auto inst = _registry->createEditor(EditorTypeId("MockState"));
        auto mock = std::dynamic_pointer_cast<MockState>(inst.state);
        mock->setDisplayName("Test Instance");
        mock->setName("custom_name");
        mock->setValue(123);

        // Set selection
        SelectionSource src{EditorInstanceId("test"), "test"};
        _registry->selectionContext()->setSelectedData(SelectedDataKey("data_key"), src);

        auto json = _registry->toJson();

        // Create new registry and restore
        EditorRegistry restored(nullptr);
        restored.registerType({
            .type_id = "MockState",
            .create_state = []() { return std::make_shared<MockState>("MockState"); },
            .create_view = [](auto) { return new QLabel(); }});

        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.stateCount() == 1);

        auto restoredStates = restored.allStates();
        auto restoredMock = std::dynamic_pointer_cast<MockState>(restoredStates[0]);
        REQUIRE(restoredMock->getDisplayName() == "Test Instance");
        REQUIRE(restoredMock->getName() == "custom_name");
        REQUIRE(restoredMock->getValue() == 123);

        REQUIRE(restored.selectionContext()->primarySelectedData().toString() == "data_key");

        delete inst.view;
    }

    SECTION("fromJson clears existing states") {
        _registry->createEditor(EditorTypeId("MockState"));
        _registry->createEditor(EditorTypeId("MockState"));
        REQUIRE(_registry->stateCount() == 2);

        EditorRegistry other(nullptr);
        other.registerType({
            .type_id = "MockState",
            .create_state = []() { return std::make_shared<MockState>("MockState"); },
            .create_view = [](auto) { return new QLabel(); }});
        other.createEditor(EditorTypeId("MockState"));
        auto json = other.toJson();

        _registry->fromJson(json);
        REQUIRE(_registry->stateCount() == 1);
    }

    SECTION("fromJson skips unknown types") {
        EditorRegistry other(nullptr);
        other.registerType({
            .type_id = "UnknownType",
            .create_state = []() { return std::make_shared<MockState>("UnknownType"); },
            .create_view = [](auto) { return new QLabel(); }});
        other.createEditor(EditorTypeId("UnknownType"));
        auto json = other.toJson();

        // _registry doesn't have "UnknownType" registered
        REQUIRE(_registry->fromJson(json));
        REQUIRE(_registry->stateCount() == 0);  // Skipped unknown type
    }
}

// === Integration Test ===

TEST_CASE_METHOD(RegistryTestFixture,
                 "EditorRegistry full workflow",
                 "[EditorRegistry][integration]") {

    SECTION("complete editor lifecycle") {
        // Register type
        _registry->registerType({
            .type_id = "WorkflowTest",
            .display_name = "Workflow Test",
            .menu_path = "View/Test",
            .default_zone = "main",
            .allow_multiple = true,
            .create_state = []() { return std::make_shared<MockState>("WorkflowTest"); },
            .create_view = [](auto s) { return new QLabel("View:" + s->getInstanceId()); },
            .create_properties = [](auto s) { return new QLabel("Props:" + s->getInstanceId()); }});

        // Create editor
        auto inst1 = _registry->createEditor(EditorTypeId("WorkflowTest"));
        REQUIRE(inst1.state != nullptr);
        REQUIRE(_registry->stateCount() == 1);

        // Create second editor (allow_multiple = true)
        auto inst2 = _registry->createEditor(EditorTypeId("WorkflowTest"));
        REQUIRE(_registry->stateCount() == 2);

        // Query by type
        auto byType = _registry->statesByType(EditorTypeId("WorkflowTest"));
        REQUIRE(byType.size() == 2);

        // Make changes
        auto mock1 = std::dynamic_pointer_cast<MockState>(inst1.state);
        mock1->setValue(100);
        REQUIRE(_registry->hasUnsavedChanges());

        // Selection
        SelectionSource src{EditorInstanceId(inst1.state->getInstanceId()), "view"};
        _registry->selectionContext()->setSelectedData(SelectedDataKey("test_data"), src);
        REQUIRE(_registry->selectionContext()->primarySelectedData().toString() == "test_data");

        // Serialize
        auto json = _registry->toJson();

        // Close one editor
        _registry->unregisterState(EditorInstanceId(inst1.state->getInstanceId()));
        REQUIRE(_registry->stateCount() == 1);

        // Restore
        _registry->fromJson(json);
        REQUIRE(_registry->stateCount() == 2);

        // Clean up
        delete inst1.view;
        delete inst1.properties;
        delete inst2.view;
        delete inst2.properties;
    }
}
