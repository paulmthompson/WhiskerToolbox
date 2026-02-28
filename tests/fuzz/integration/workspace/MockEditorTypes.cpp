/**
 * @file MockEditorTypes.cpp
 * @brief Implementation of mock EditorState types for registry fuzz testing
 *
 * Phase 3 of the Workspace Fuzz Testing Roadmap.
 */

#include "MockEditorTypes.hpp"

#include "EditorState/ZoneTypes.hpp"

#include <QWidget>

#include <algorithm>

// ============================================================================
// MockEditorStateA
// ============================================================================

MockEditorStateA::MockEditorStateA(QObject * parent)
    : EditorState(parent) {
}

std::string MockEditorStateA::toJson() const {
    MockStateDataA data = _data;
    data.instance_id = getInstanceId().toStdString();
    data.display_name = getDisplayName().toStdString();
    return rfl::json::write(data);
}

bool MockEditorStateA::fromJson(std::string const & json) {
    auto result = rfl::json::read<MockStateDataA>(json);
    if (!result) {
        return false;
    }

    _data = *result;

    if (!_data.instance_id.empty()) {
        setInstanceId(QString::fromStdString(_data.instance_id));
    }
    if (!_data.display_name.empty()) {
        setDisplayName(QString::fromStdString(_data.display_name));
    }

    markDirty();
    return true;
}

void MockEditorStateA::setIntField(int value) {
    if (_data.int_field != value) {
        _data.int_field = value;
        markDirty();
    }
}

void MockEditorStateA::setDoubleField(double value) {
    if (_data.double_field != value) {
        _data.double_field = value;
        markDirty();
    }
}

void MockEditorStateA::setBoolField(bool value) {
    if (_data.bool_field != value) {
        _data.bool_field = value;
        markDirty();
    }
}

// ============================================================================
// MockEditorStateB
// ============================================================================

MockEditorStateB::MockEditorStateB(QObject * parent)
    : EditorState(parent) {
}

std::string MockEditorStateB::toJson() const {
    MockStateDataB data = _data;
    data.instance_id = getInstanceId().toStdString();
    data.display_name = getDisplayName().toStdString();
    return rfl::json::write(data);
}

bool MockEditorStateB::fromJson(std::string const & json) {
    auto result = rfl::json::read<MockStateDataB>(json);
    if (!result) {
        return false;
    }

    _data = *result;

    if (!_data.instance_id.empty()) {
        setInstanceId(QString::fromStdString(_data.instance_id));
    }
    if (!_data.display_name.empty()) {
        setDisplayName(QString::fromStdString(_data.display_name));
    }

    markDirty();
    return true;
}

void MockEditorStateB::setStringField(std::string const & value) {
    if (_data.string_field != value) {
        _data.string_field = value;
        markDirty();
    }
}

void MockEditorStateB::setListField(std::vector<std::string> const & value) {
    _data.list_field = value;
    markDirty();
}

void MockEditorStateB::setOptionalField(std::optional<std::string> const & value) {
    _data.optional_field = value;
    markDirty();
}

void MockEditorStateB::setOptionalInt(std::optional<int> const & value) {
    _data.optional_int = value;
    markDirty();
}

// ============================================================================
// MockEditorStateC
// ============================================================================

MockEditorStateC::MockEditorStateC(QObject * parent)
    : EditorState(parent) {
}

std::string MockEditorStateC::toJson() const {
    MockStateDataC data = _data;
    data.instance_id = getInstanceId().toStdString();
    data.display_name = getDisplayName().toStdString();
    return rfl::json::write(data);
}

bool MockEditorStateC::fromJson(std::string const & json) {
    auto result = rfl::json::read<MockStateDataC>(json);
    if (!result) {
        return false;
    }

    _data = *result;

    if (!_data.instance_id.empty()) {
        setInstanceId(QString::fromStdString(_data.instance_id));
    }
    if (!_data.display_name.empty()) {
        setDisplayName(QString::fromStdString(_data.display_name));
    }

    markDirty();
    return true;
}

void MockEditorStateC::setNested(MockNestedData const & value) {
    _data.nested = value;
    markDirty();
}

void MockEditorStateC::setNestedList(std::vector<MockNestedData> const & value) {
    _data.nested_list = value;
    markDirty();
}

// ============================================================================
// Registration helpers
// ============================================================================

void registerMockTypes(EditorRegistry * registry) {
    registerMockTypes(registry, 3);
}

void registerMockTypes(EditorRegistry * registry, int count) {
    registerMockTypes(registry, count, false);
}

void registerMockTypes(EditorRegistry * registry, int count, bool with_widget_factories) {
    count = std::clamp(count, 0, 3);

    // Factory that produces minimal QWidget instances for fuzz testing
    // (no actual UI logic, just enough for ADS dock wrapping)
    auto make_view = [](std::shared_ptr<EditorState> /*state*/) -> QWidget * {
        return new QWidget();
    };
    auto make_props = [](std::shared_ptr<EditorState> /*state*/) -> QWidget * {
        return new QWidget();
    };

    if (count >= 1) {
        EditorRegistry::EditorTypeInfo info{
            .type_id = "MockTypeA",
            .display_name = "Mock Widget A",
            .preferred_zone = Zone::Left,
            .properties_zone = Zone::Right,
            .allow_multiple = false,
            .create_state = [] { return std::make_shared<MockEditorStateA>(); },
        };
        if (with_widget_factories) {
            info.create_view = make_view;
            info.create_properties = make_props;
        }
        registry->registerType(std::move(info));
    }

    if (count >= 2) {
        EditorRegistry::EditorTypeInfo info{
            .type_id = "MockTypeB",
            .display_name = "Mock Widget B",
            .preferred_zone = Zone::Center,
            .properties_zone = Zone::Right,
            .allow_multiple = true,
            .create_state = [] { return std::make_shared<MockEditorStateB>(); },
        };
        if (with_widget_factories) {
            info.create_view = make_view;
            info.create_properties = make_props;
        }
        registry->registerType(std::move(info));
    }

    if (count >= 3) {
        EditorRegistry::EditorTypeInfo info{
            .type_id = "MockTypeC",
            .display_name = "Mock Widget C",
            .preferred_zone = Zone::Center,
            .properties_zone = Zone::Right,
            .allow_multiple = false,
            .create_state = [] { return std::make_shared<MockEditorStateC>(); },
        };
        if (with_widget_factories) {
            info.create_view = make_view;
            info.create_properties = make_props;
        }
        registry->registerType(std::move(info));
    }
}
