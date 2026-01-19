#include "WorkspaceManager.hpp"

#include "EditorState.hpp"
#include "SelectionContext.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <algorithm>

// === Serialization Structures ===

namespace {

struct SerializedState {
    std::string type_name;
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

}// namespace

// === WorkspaceManager Implementation ===

WorkspaceManager::WorkspaceManager(std::shared_ptr<DataManager> data_manager, QObject * parent)
    : QObject(parent),
      _data_manager(std::move(data_manager)),
      _selection_context(std::make_unique<SelectionContext>(this)) {
}

WorkspaceManager::~WorkspaceManager() = default;

// === Editor Type Registration ===

void WorkspaceManager::registerEditorType(EditorTypeInfo const & info, EditorStateFactory factory) {
    auto key = info.type_name.toStdString();
    _factories[key] = std::move(factory);
    _type_info[key] = info;
}

std::vector<EditorTypeInfo> WorkspaceManager::registeredEditorTypes() const {
    std::vector<EditorTypeInfo> result;
    result.reserve(_type_info.size());
    for (auto const & [key, info] : _type_info) {
        result.push_back(info);
    }
    return result;
}

bool WorkspaceManager::isEditorTypeRegistered(QString const & type_name) const {
    return _factories.contains(type_name.toStdString());
}

// === State Factory ===

std::shared_ptr<EditorState> WorkspaceManager::createState(QString const & type_name) {
    auto it = _factories.find(type_name.toStdString());
    if (it == _factories.end()) {
        return nullptr;
    }

    auto state = it->second();
    if (state) {
        registerState(state);
    }
    return state;
}

// === State Registry ===

void WorkspaceManager::registerState(std::shared_ptr<EditorState> state) {
    if (!state) {
        return;
    }

    auto instance_id = state->getInstanceId().toStdString();

    // Check if already registered
    if (_states.contains(instance_id)) {
        return;
    }

    _states[instance_id] = state;
    connectStateSignals(state.get());

    emit stateRegistered(state->getInstanceId(), state->getTypeName());
    emit workspaceChanged();
}

void WorkspaceManager::unregisterState(QString const & instance_id) {
    auto it = _states.find(instance_id.toStdString());
    if (it == _states.end()) {
        return;
    }

    auto state = it->second;

    // Disconnect signals
    disconnect(state.get(), nullptr, this, nullptr);

    _states.erase(it);

    emit stateUnregistered(instance_id);
    emit workspaceChanged();

    // Check if unsaved changes status changed
    emit unsavedChangesChanged(hasUnsavedChanges());
}

std::shared_ptr<EditorState> WorkspaceManager::getState(QString const & instance_id) const {
    auto it = _states.find(instance_id.toStdString());
    if (it != _states.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<EditorState>>
WorkspaceManager::getStatesByType(QString const & type_name) const {
    std::vector<std::shared_ptr<EditorState>> result;
    for (auto const & [id, state] : _states) {
        if (state->getTypeName() == type_name) {
            result.push_back(state);
        }
    }
    return result;
}

std::vector<std::shared_ptr<EditorState>> WorkspaceManager::getAllStates() const {
    std::vector<std::shared_ptr<EditorState>> result;
    result.reserve(_states.size());
    for (auto const & [id, state] : _states) {
        result.push_back(state);
    }
    return result;
}

size_t WorkspaceManager::stateCount() const {
    return _states.size();
}

// === Selection Context ===

SelectionContext * WorkspaceManager::selectionContext() const {
    return _selection_context.get();
}

// === Data Manager Access ===

std::shared_ptr<DataManager> WorkspaceManager::dataManager() const {
    return _data_manager;
}

// === Serialization ===

std::string WorkspaceManager::toJson() const {
    SerializedWorkspace workspace;

    // Serialize all states
    for (auto const & [id, state] : _states) {
        SerializedState serialized;
        serialized.type_name = state->getTypeName().toStdString();
        serialized.instance_id = state->getInstanceId().toStdString();
        serialized.display_name = state->getDisplayName().toStdString();
        serialized.state_json = state->toJson();
        workspace.states.push_back(std::move(serialized));
    }

    // Serialize selection state
    workspace.primary_selection = _selection_context->primarySelectedData().toStdString();
    for (auto const & key : _selection_context->allSelectedData()) {
        workspace.all_selections.push_back(key.toStdString());
    }

    return rfl::json::write(workspace);
}

bool WorkspaceManager::fromJson(std::string const & json) {
    auto result = rfl::json::read<SerializedWorkspace>(json);
    if (!result) {
        return false;
    }

    auto const & workspace = *result;

    // Clear existing states
    auto old_states = getAllStates();
    for (auto const & state : old_states) {
        unregisterState(state->getInstanceId());
    }

    // Restore states
    for (auto const & serialized : workspace.states) {
        auto type_name = QString::fromStdString(serialized.type_name);

        // Create state via factory
        auto it = _factories.find(serialized.type_name);
        if (it == _factories.end()) {
            // Unknown type, skip
            continue;
        }

        auto state = it->second();
        if (!state) {
            continue;
        }

        // Restore state from JSON
        if (!state->fromJson(serialized.state_json)) {
            continue;
        }

        // Restore display name (may not be in state_json)
        state->setDisplayName(QString::fromStdString(serialized.display_name));

        registerState(state);
    }

    // Restore selection
    SelectionSource source{"WorkspaceManager", "fromJson"};
    _selection_context->clearSelection(source);
    for (auto const & key : workspace.all_selections) {
        _selection_context->addToSelection(QString::fromStdString(key), source);
    }
    if (!workspace.primary_selection.empty()) {
        _selection_context->setSelectedData(QString::fromStdString(workspace.primary_selection),
                                            source);
    }

    return true;
}

bool WorkspaceManager::hasUnsavedChanges() const {
    return std::any_of(_states.begin(), _states.end(), [](auto const & pair) {
        return pair.second->isDirty();
    });
}

void WorkspaceManager::markAllClean() {
    for (auto & [id, state] : _states) {
        state->markClean();
    }
}

// === Signal Handlers ===

void WorkspaceManager::onStateChanged() {
    emit workspaceChanged();
}

void WorkspaceManager::onStateDirtyChanged(bool is_dirty) {
    emit unsavedChangesChanged(hasUnsavedChanges());
}

// === Private Helpers ===

void WorkspaceManager::connectStateSignals(EditorState * state) {
    connect(state, &EditorState::stateChanged, this, &WorkspaceManager::onStateChanged);

    connect(state, &EditorState::dirtyChanged, this, &WorkspaceManager::onStateDirtyChanged);
}
