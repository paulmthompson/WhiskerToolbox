#include "EditorRegistry.hpp"

#include "EditorState.hpp"
#include "SelectionContext.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <algorithm>
#include <iostream>

// === Serialization Structures ===

namespace {

struct SerializedState {
    std::string type_id;
    std::string instance_id;
    std::string display_name;
    std::string state_json;
};

struct SerializedWorkspace {
    std::string version = "1.0";
    std::vector<SerializedState> states;
    std::string primary_selection;
    std::vector<std::string> all_selections;
};

}  // namespace

// === EditorRegistry Implementation ===

EditorRegistry::EditorRegistry(std::shared_ptr<DataManager> data_manager, QObject * parent)
    : QObject(parent)
    , _data_manager(std::move(data_manager))
    , _selection_context(std::make_unique<SelectionContext>(this)) {
}

EditorRegistry::~EditorRegistry() = default;

// === Type Registration ===

bool EditorRegistry::registerType(EditorTypeInfo info) {
    if (info.type_id.isEmpty()) {
        std::cerr << "EditorRegistry::registerType: type_id cannot be empty" << std::endl;
        return false;
    }

    if (!info.create_state) {
        std::cerr << "EditorRegistry::registerType: create_state cannot be null for "
                  << info.type_id.toStdString() << std::endl;
        return false;
    }

    // Note: create_view is optional for state-only registrations (e.g., testing)
    // createEditor() will fail if called without create_view, but createState() will work

    if (_types.contains(info.type_id)) {
        std::cerr << "EditorRegistry::registerType: type_id already registered: "
                  << info.type_id.toStdString() << std::endl;
        return false;
    }

    QString type_id = info.type_id;
    _types[type_id] = std::move(info);

    emit typeRegistered(type_id);
    return true;
}

bool EditorRegistry::unregisterType(QString const & type_id) {
    auto it = _types.find(type_id);
    if (it == _types.end()) {
        return false;
    }

    _types.erase(it);
    emit typeUnregistered(type_id);
    return true;
}

bool EditorRegistry::hasType(QString const & type_id) const {
    return _types.contains(type_id);
}

EditorRegistry::EditorTypeInfo EditorRegistry::typeInfo(QString const & type_id) const {
    auto it = _types.find(type_id);
    if (it == _types.end()) {
        return EditorTypeInfo{};
    }
    return it->second;
}

std::vector<EditorRegistry::EditorTypeInfo> EditorRegistry::allTypes() const {
    std::vector<EditorTypeInfo> result;
    result.reserve(_types.size());
    for (auto const & [id, info] : _types) {
        result.push_back(info);
    }
    return result;
}

std::vector<EditorRegistry::EditorTypeInfo>
EditorRegistry::typesByMenuPath(QString const & path) const {
    std::vector<EditorTypeInfo> result;
    for (auto const & [id, info] : _types) {
        if (info.menu_path == path) {
            result.push_back(info);
        }
    }
    return result;
}

// === Editor Creation ===

EditorRegistry::EditorInstance EditorRegistry::createEditor(QString const & type_id) {
    auto it = _types.find(type_id);
    if (it == _types.end()) {
        std::cerr << "EditorRegistry::createEditor: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return EditorInstance{nullptr, nullptr, nullptr};
    }

    auto const & type = it->second;

    // Check that we have a view factory (required for createEditor)
    if (!type.create_view) {
        std::cerr << "EditorRegistry::createEditor: create_view is null for "
                  << type_id.toStdString() << ". Use createState() for state-only types." << std::endl;
        return EditorInstance{nullptr, nullptr, nullptr};
    }

    // Create state
    auto state = type.create_state();
    if (!state) {
        std::cerr << "EditorRegistry::createEditor: create_state returned nullptr for "
                  << type_id.toStdString() << std::endl;
        return EditorInstance{nullptr, nullptr, nullptr};
    }

    // Register state
    registerState(state);

    // Create view
    auto * view = type.create_view(state);
    if (!view) {
        std::cerr << "EditorRegistry::createEditor: create_view returned nullptr for "
                  << type_id.toStdString() << std::endl;
        unregisterState(state->getInstanceId());
        return EditorInstance{nullptr, nullptr, nullptr};
    }

    // Create properties (optional)
    QWidget * properties = nullptr;
    if (type.create_properties) {
        properties = type.create_properties(state);
    }

    emit editorCreated(state->getInstanceId(), type_id);

    return EditorInstance{
        .state = std::move(state),
        .view = view,
        .properties = properties};
}

std::shared_ptr<EditorState> EditorRegistry::createState(QString const & type_id) {
    auto it = _types.find(type_id);
    if (it == _types.end()) {
        std::cerr << "EditorRegistry::createState: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return nullptr;
    }

    return it->second.create_state();
}

QWidget * EditorRegistry::createView(std::shared_ptr<EditorState> state) {
    if (!state) {
        return nullptr;
    }

    QString type_id = state->getTypeName();
    auto it = _types.find(type_id);
    if (it == _types.end()) {
        std::cerr << "EditorRegistry::createView: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return nullptr;
    }

    return it->second.create_view(state);
}

QWidget * EditorRegistry::createProperties(std::shared_ptr<EditorState> state) {
    if (!state) {
        return nullptr;
    }

    QString type_id = state->getTypeName();
    auto it = _types.find(type_id);
    if (it == _types.end()) {
        std::cerr << "EditorRegistry::createProperties: unknown type_id: "
                  << type_id.toStdString() << std::endl;
        return nullptr;
    }

    if (!it->second.create_properties) {
        return nullptr;  // No properties factory - valid
    }

    return it->second.create_properties(state);
}

// === State Registry ===

void EditorRegistry::registerState(std::shared_ptr<EditorState> state) {
    if (!state) {
        return;
    }

    QString instance_id = state->getInstanceId();

    if (_states.contains(instance_id)) {
        return;  // Already registered
    }

    _states[instance_id] = state;
    connectStateSignals(state.get());

    emit stateRegistered(instance_id, state->getTypeName());
    emit workspaceChanged();
}

void EditorRegistry::unregisterState(QString const & instance_id) {
    auto it = _states.find(instance_id);
    if (it == _states.end()) {
        return;
    }

    auto state = it->second;
    disconnect(state.get(), nullptr, this, nullptr);

    _states.erase(it);

    emit stateUnregistered(instance_id);
    emit workspaceChanged();
    emit unsavedChangesChanged(hasUnsavedChanges());
}

std::shared_ptr<EditorState> EditorRegistry::state(QString const & instance_id) const {
    auto it = _states.find(instance_id);
    if (it != _states.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<EditorState>>
EditorRegistry::statesByType(QString const & type_id) const {
    std::vector<std::shared_ptr<EditorState>> result;
    for (auto const & [id, s] : _states) {
        if (s->getTypeName() == type_id) {
            result.push_back(s);
        }
    }
    return result;
}

std::vector<std::shared_ptr<EditorState>> EditorRegistry::allStates() const {
    std::vector<std::shared_ptr<EditorState>> result;
    result.reserve(_states.size());
    for (auto const & [id, s] : _states) {
        result.push_back(s);
    }
    return result;
}

size_t EditorRegistry::stateCount() const {
    return _states.size();
}

// === Selection & Data ===

SelectionContext * EditorRegistry::selectionContext() const {
    return _selection_context.get();
}

std::shared_ptr<DataManager> EditorRegistry::dataManager() const {
    return _data_manager;
}

// === Serialization ===

std::string EditorRegistry::toJson() const {
    SerializedWorkspace workspace;

    for (auto const & [id, s] : _states) {
        SerializedState serialized;
        serialized.type_id = s->getTypeName().toStdString();
        serialized.instance_id = s->getInstanceId().toStdString();
        serialized.display_name = s->getDisplayName().toStdString();
        serialized.state_json = s->toJson();
        workspace.states.push_back(std::move(serialized));
    }

    workspace.primary_selection = _selection_context->primarySelectedData().toStdString();
    for (auto const & key : _selection_context->allSelectedData()) {
        workspace.all_selections.push_back(key.toStdString());
    }

    return rfl::json::write(workspace);
}

bool EditorRegistry::fromJson(std::string const & json) {
    auto result = rfl::json::read<SerializedWorkspace>(json);
    if (!result) {
        std::cerr << "EditorRegistry::fromJson: failed to parse JSON" << std::endl;
        return false;
    }

    auto const & workspace = *result;

    // Clear existing states
    auto old_states = allStates();
    for (auto const & s : old_states) {
        unregisterState(s->getInstanceId());
    }

    // Restore states
    for (auto const & serialized : workspace.states) {
        QString type_id = QString::fromStdString(serialized.type_id);

        auto it = _types.find(type_id);
        if (it == _types.end()) {
            std::cerr << "EditorRegistry::fromJson: unknown type: "
                      << serialized.type_id << ", skipping" << std::endl;
            continue;
        }

        auto state = it->second.create_state();
        if (!state) {
            continue;
        }

        if (!state->fromJson(serialized.state_json)) {
            std::cerr << "EditorRegistry::fromJson: failed to restore state for "
                      << serialized.instance_id << std::endl;
            continue;
        }

        state->setDisplayName(QString::fromStdString(serialized.display_name));
        registerState(state);
    }

    // Restore selection
    SelectionSource source{"EditorRegistry", "fromJson"};
    _selection_context->clearSelection(source);
    for (auto const & key : workspace.all_selections) {
        _selection_context->addToSelection(QString::fromStdString(key), source);
    }
    if (!workspace.primary_selection.empty()) {
        _selection_context->setSelectedData(
            QString::fromStdString(workspace.primary_selection), source);
    }

    return true;
}

bool EditorRegistry::hasUnsavedChanges() const {
    return std::any_of(_states.begin(), _states.end(),
                       [](auto const & pair) { return pair.second->isDirty(); });
}

void EditorRegistry::markAllClean() {
    for (auto & [id, s] : _states) {
        s->markClean();
    }
}

// === Private ===

void EditorRegistry::onStateChanged() {
    emit workspaceChanged();
}

void EditorRegistry::onStateDirtyChanged(bool is_dirty) {
    emit unsavedChangesChanged(hasUnsavedChanges());
}

void EditorRegistry::connectStateSignals(EditorState * state) {
    connect(state, &EditorState::stateChanged, this, &EditorRegistry::onStateChanged);
    connect(state, &EditorState::dirtyChanged, this, &EditorRegistry::onStateDirtyChanged);
}
