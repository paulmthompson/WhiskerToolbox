/**
 * @file EditorFactory.test.cpp
 * @brief Unit tests for EditorFactory
 */

#include "EditorState/EditorFactory.hpp"
#include "EditorState/EditorState.hpp"
#include "EditorState/WorkspaceManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QLabel>
#include <QSignalSpy>

#include <memory>

// === Test EditorState Implementation ===

/**
 * @brief Simple test state for factory testing
 */
class MockEditorState : public EditorState {
    Q_OBJECT

public:
    explicit MockEditorState(QString type_name = "MockEditor", QObject * parent = nullptr)
        : EditorState(parent)
        , _type_name(std::move(type_name)) {}

    [[nodiscard]] QString getTypeName() const override { return _type_name; }

    [[nodiscard]] std::string toJson() const override {
        return R"({"value": )" + std::to_string(_value) + "}";
    }

    bool fromJson(std::string const & json) override {
        // Simple parse - just look for value
        auto pos = json.find("\"value\":");
        if (pos != std::string::npos) {
            _value = std::stoi(json.substr(pos + 8));
            return true;
        }
        return false;
    }

    void setValue(int v) {
        _value = v;
        markDirty();
    }
    [[nodiscard]] int value() const { return _value; }

private:
    QString _type_name;
    int _value = 0;
};

// === Test Fixtures ===

class EditorFactoryTestFixture {
public:
    EditorFactoryTestFixture() {
        // Ensure QApplication exists for Qt tests
        if (!QApplication::instance()) {
            int argc = 0;
            _app = std::make_unique<QApplication>(argc, nullptr);
        }

        _data_manager = nullptr; // Not needed for these tests
        _workspace_manager = std::make_unique<WorkspaceManager>(_data_manager);
        _factory =
            std::make_unique<EditorFactory>(_workspace_manager.get(), _data_manager);
    }

protected:
    std::unique_ptr<QApplication> _app;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<WorkspaceManager> _workspace_manager;
    std::unique_ptr<EditorFactory> _factory;
};

// === Registration Tests ===

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory registration works",
                 "[EditorFactory][registration]") {

    SECTION("can register editor type") {
        bool result = _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{
                .type_id = "TestEditor",
                .display_name = "Test Editor",
                .icon_path = "",
                .menu_path = "View/Test",
                .default_zone = "main",
                .allow_multiple = true},
            []() { return std::make_shared<MockEditorState>("TestEditor"); },
            [](auto state) { return new QLabel("View"); },
            [](auto state) { return new QLabel("Properties"); });

        REQUIRE(result == true);
        REQUIRE(_factory->hasEditorType("TestEditor") == true);
    }

    SECTION("cannot register same type twice") {
        _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "DuplicateEditor"},
            []() { return std::make_shared<MockEditorState>("DuplicateEditor"); },
            [](auto state) { return new QLabel("View"); });

        bool result = _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "DuplicateEditor"},
            []() { return std::make_shared<MockEditorState>("DuplicateEditor"); },
            [](auto state) { return new QLabel("View"); });

        REQUIRE(result == false);
    }

    SECTION("cannot register with empty type_id") {
        bool result = _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = ""},
            []() { return std::make_shared<MockEditorState>(); },
            [](auto state) { return new QLabel("View"); });

        REQUIRE(result == false);
    }

    SECTION("cannot register with null state factory") {
        bool result = _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "NullState"},
            nullptr,
            [](auto state) { return new QLabel("View"); });

        REQUIRE(result == false);
    }

    SECTION("cannot register with null view factory") {
        bool result = _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "NullView"},
            []() { return std::make_shared<MockEditorState>(); },
            nullptr);

        REQUIRE(result == false);
    }

    SECTION("can register without properties factory") {
        bool result = _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "NoProperties"},
            []() { return std::make_shared<MockEditorState>("NoProperties"); },
            [](auto state) { return new QLabel("View"); });
        // No properties factory provided

        REQUIRE(result == true);
    }

    SECTION("emits signal on registration") {
        QSignalSpy spy(_factory.get(), &EditorFactory::editorTypeRegistered);

        _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "SignalEditor"},
            []() { return std::make_shared<MockEditorState>("SignalEditor"); },
            [](auto state) { return new QLabel("View"); });

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "SignalEditor");
    }
}

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory unregistration works",
                 "[EditorFactory][registration]") {

    SECTION("can unregister existing type") {
        _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "ToUnregister"},
            []() { return std::make_shared<MockEditorState>("ToUnregister"); },
            [](auto state) { return new QLabel("View"); });

        REQUIRE(_factory->hasEditorType("ToUnregister") == true);

        bool result = _factory->unregisterEditorType("ToUnregister");

        REQUIRE(result == true);
        REQUIRE(_factory->hasEditorType("ToUnregister") == false);
    }

    SECTION("returns false for non-existent type") {
        bool result = _factory->unregisterEditorType("NonExistent");
        REQUIRE(result == false);
    }

    SECTION("emits signal on unregistration") {
        _factory->registerEditorType(
            EditorFactory::EditorTypeInfo{.type_id = "ToUnregister2"},
            []() { return std::make_shared<MockEditorState>("ToUnregister2"); },
            [](auto state) { return new QLabel("View"); });

        QSignalSpy spy(_factory.get(), &EditorFactory::editorTypeUnregistered);

        _factory->unregisterEditorType("ToUnregister2");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "ToUnregister2");
    }
}

// === Creation Tests ===

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory creation works",
                 "[EditorFactory][creation]") {

    // Register a test editor
    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{
            .type_id = "CreateTest",
            .display_name = "Create Test",
            .default_zone = "main",
            .allow_multiple = true},
        []() { return std::make_shared<MockEditorState>("CreateTest"); },
        [](auto state) {
            auto * label = new QLabel("View");
            label->setObjectName("TestView");
            return label;
        },
        [](auto state) {
            auto * label = new QLabel("Properties");
            label->setObjectName("TestProperties");
            return label;
        });

    SECTION("createEditor returns all components") {
        auto instance = _factory->createEditor("CreateTest");

        REQUIRE(instance.state != nullptr);
        REQUIRE(instance.view != nullptr);
        REQUIRE(instance.properties != nullptr);

        REQUIRE(instance.state->getTypeName() == "CreateTest");
        REQUIRE(instance.view->objectName() == "TestView");
        REQUIRE(instance.properties->objectName() == "TestProperties");

        // Clean up widgets
        delete instance.view;
        delete instance.properties;
    }

    SECTION("createEditor registers state with WorkspaceManager") {
        auto instance = _factory->createEditor("CreateTest");

        auto found = _workspace_manager->getState(instance.state->getInstanceId());
        REQUIRE(found == instance.state);

        delete instance.view;
        delete instance.properties;
    }

    SECTION("createEditor returns empty for unknown type") {
        auto instance = _factory->createEditor("UnknownType");

        REQUIRE(instance.state == nullptr);
        REQUIRE(instance.view == nullptr);
        REQUIRE(instance.properties == nullptr);
    }

    SECTION("createEditor emits signal") {
        QSignalSpy spy(_factory.get(), &EditorFactory::editorCreated);

        auto instance = _factory->createEditor("CreateTest");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == instance.state->getInstanceId());
        REQUIRE(spy.at(0).at(1).toString() == "CreateTest");

        delete instance.view;
        delete instance.properties;
    }
}

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory createState works",
                 "[EditorFactory][creation]") {

    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{.type_id = "StateOnly"},
        []() { return std::make_shared<MockEditorState>("StateOnly"); },
        [](auto state) { return new QLabel("View"); });

    SECTION("createState returns state without registering") {
        auto state = _factory->createState("StateOnly");

        REQUIRE(state != nullptr);
        REQUIRE(state->getTypeName() == "StateOnly");

        // Should NOT be registered
        auto found = _workspace_manager->getState(state->getInstanceId());
        REQUIRE(found == nullptr);
    }

    SECTION("createState returns nullptr for unknown type") {
        auto state = _factory->createState("UnknownType");
        REQUIRE(state == nullptr);
    }
}

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory createView works",
                 "[EditorFactory][creation]") {

    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{.type_id = "ViewOnly"},
        []() { return std::make_shared<MockEditorState>("ViewOnly"); },
        [](auto state) {
            auto * label = new QLabel("View for " + state->getInstanceId());
            return label;
        });

    SECTION("createView creates view for existing state") {
        auto state = std::make_shared<MockEditorState>("ViewOnly");
        auto * view = _factory->createView(state);

        REQUIRE(view != nullptr);
        auto * label = qobject_cast<QLabel *>(view);
        REQUIRE(label != nullptr);
        REQUIRE(label->text().contains(state->getInstanceId()));

        delete view;
    }

    SECTION("createView returns nullptr for null state") {
        auto * view = _factory->createView(nullptr);
        REQUIRE(view == nullptr);
    }

    SECTION("createView returns nullptr for unregistered type") {
        auto state = std::make_shared<MockEditorState>("UnknownType");
        auto * view = _factory->createView(state);
        REQUIRE(view == nullptr);
    }
}

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory createProperties works",
                 "[EditorFactory][creation]") {

    // Editor with properties
    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{.type_id = "WithProps"},
        []() { return std::make_shared<MockEditorState>("WithProps"); },
        [](auto state) { return new QLabel("View"); },
        [](auto state) { return new QLabel("Props for " + state->getInstanceId()); });

    // Editor without properties
    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{.type_id = "NoProps"},
        []() { return std::make_shared<MockEditorState>("NoProps"); },
        [](auto state) { return new QLabel("View"); });

    SECTION("createProperties creates properties for existing state") {
        auto state = std::make_shared<MockEditorState>("WithProps");
        auto * props = _factory->createProperties(state);

        REQUIRE(props != nullptr);
        auto * label = qobject_cast<QLabel *>(props);
        REQUIRE(label != nullptr);
        REQUIRE(label->text().contains(state->getInstanceId()));

        delete props;
    }

    SECTION("createProperties returns nullptr when no factory") {
        auto state = std::make_shared<MockEditorState>("NoProps");
        auto * props = _factory->createProperties(state);
        REQUIRE(props == nullptr);
    }

    SECTION("createProperties returns nullptr for null state") {
        auto * props = _factory->createProperties(nullptr);
        REQUIRE(props == nullptr);
    }
}

// === Query Tests ===

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory queries work",
                 "[EditorFactory][queries]") {

    // Register some editors
    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{
            .type_id = "Editor1",
            .display_name = "Editor One",
            .menu_path = "View/Group1",
            .default_zone = "main"},
        []() { return std::make_shared<MockEditorState>("Editor1"); },
        [](auto state) { return new QLabel("View"); });

    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{
            .type_id = "Editor2",
            .display_name = "Editor Two",
            .menu_path = "View/Group1",
            .default_zone = "right"},
        []() { return std::make_shared<MockEditorState>("Editor2"); },
        [](auto state) { return new QLabel("View"); });

    _factory->registerEditorType(
        EditorFactory::EditorTypeInfo{
            .type_id = "Editor3",
            .display_name = "Editor Three",
            .menu_path = "View/Group2",
            .default_zone = "main"},
        []() { return std::make_shared<MockEditorState>("Editor3"); },
        [](auto state) { return new QLabel("View"); });

    SECTION("getEditorInfo returns correct info") {
        auto info = _factory->getEditorInfo("Editor1");

        REQUIRE(info.type_id == "Editor1");
        REQUIRE(info.display_name == "Editor One");
        REQUIRE(info.menu_path == "View/Group1");
        REQUIRE(info.default_zone == "main");
    }

    SECTION("getEditorInfo returns empty for unknown type") {
        auto info = _factory->getEditorInfo("UnknownType");
        REQUIRE(info.type_id.isEmpty());
    }

    SECTION("availableEditors returns all registered") {
        auto editors = _factory->availableEditors();
        REQUIRE(editors.size() == 3);
    }

    SECTION("editorsByMenuPath filters correctly") {
        auto group1 = _factory->editorsByMenuPath("View/Group1");
        REQUIRE(group1.size() == 2);

        auto group2 = _factory->editorsByMenuPath("View/Group2");
        REQUIRE(group2.size() == 1);

        auto empty = _factory->editorsByMenuPath("View/NoGroup");
        REQUIRE(empty.empty());
    }
}

// === Accessor Tests ===

TEST_CASE_METHOD(EditorFactoryTestFixture,
                 "EditorFactory accessors work",
                 "[EditorFactory][accessors]") {

    SECTION("workspaceManager returns correct instance") {
        REQUIRE(_factory->workspaceManager() == _workspace_manager.get());
    }

    SECTION("dataManager returns correct instance") {
        REQUIRE(_factory->dataManager() == _data_manager);
    }
}

// Required for MOC
#include "EditorFactory.test.moc"
