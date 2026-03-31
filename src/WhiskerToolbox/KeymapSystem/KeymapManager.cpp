/**
 * @file KeymapManager.cpp
 * @brief Implementation of the central keymap manager
 */

#include "KeymapSystem/KeymapManager.hpp"

#include "KeymapSystem/KeyActionAdapter.hpp"

#include <algorithm>
#include <cassert>

namespace KeymapSystem {

KeymapManager::KeymapManager(QObject * parent)
    : QObject(parent) {}

KeymapManager::~KeymapManager() = default;

// --- Registration ---

bool KeymapManager::registerAction(KeyActionDescriptor const & descriptor) {
    assert(!descriptor.action_id.isEmpty() && "registerAction: action_id must not be empty");

    if (_actions.contains(descriptor.action_id)) {
        return false;
    }
    _actions.emplace(descriptor.action_id, descriptor);
    emit bindingsChanged();
    return true;
}

bool KeymapManager::unregisterAction(QString const & action_id) {
    auto const it = _actions.find(action_id);
    if (it == _actions.end()) {
        return false;
    }
    _actions.erase(it);
    _overrides.erase(action_id);
    emit bindingsChanged();
    return true;
}

// --- Query ---

std::vector<KeyActionDescriptor> KeymapManager::allActions() const {
    std::vector<KeyActionDescriptor> result;
    result.reserve(_actions.size());
    for (auto const & [id, desc]: _actions) {
        result.push_back(desc);
    }
    return result;
}

std::optional<KeyActionDescriptor> KeymapManager::action(QString const & action_id) const {
    auto const it = _actions.find(action_id);
    if (it == _actions.end()) {
        return std::nullopt;
    }
    return it->second;
}

QKeySequence KeymapManager::bindingFor(QString const & action_id) const {
    // Check for user override first
    auto const override_it = _overrides.find(action_id);
    if (override_it != _overrides.end()) {
        return override_it->second.key_sequence;
    }

    // Fall back to default binding
    auto const action_it = _actions.find(action_id);
    if (action_it != _actions.end()) {
        return action_it->second.default_binding;
    }

    return {};
}

bool KeymapManager::isActionEnabled(QString const & action_id) const {
    auto const override_it = _overrides.find(action_id);
    if (override_it != _overrides.end()) {
        return override_it->second.enabled;
    }
    // Default: enabled if action exists
    return _actions.contains(action_id);
}

// --- Override management ---

void KeymapManager::setUserOverride(QString const & action_id, QKeySequence const & key) {
    auto & state = _overrides[action_id];
    state.key_sequence = key;
    // Preserve existing enabled state if override already existed
    emit bindingsChanged();
}

void KeymapManager::clearUserOverride(QString const & action_id) {
    if (_overrides.erase(action_id) > 0) {
        emit bindingsChanged();
    }
}

void KeymapManager::clearAllOverrides() {
    if (!_overrides.empty()) {
        _overrides.clear();
        emit bindingsChanged();
    }
}

void KeymapManager::setActionEnabled(QString const & action_id, bool enabled) {
    _overrides[action_id].enabled = enabled;
    emit bindingsChanged();
}

// --- Conflict detection ---

std::vector<KeyConflict> KeymapManager::detectConflicts() const {
    std::vector<KeyConflict> conflicts;

    // Build a list of (effective_key, scope, action_id) tuples
    struct BindingInfo {
        QKeySequence key;
        KeyActionScope scope;
        QString action_id;
    };

    std::vector<BindingInfo> bindings;
    bindings.reserve(_actions.size());

    for (auto const & [id, desc]: _actions) {
        if (!isActionEnabled(id)) {
            continue;
        }
        QKeySequence const key = bindingFor(id);
        if (key.isEmpty()) {
            continue;
        }
        bindings.push_back({key, desc.scope, id});
    }

    // Compare all pairs
    for (size_t i = 0; i < bindings.size(); ++i) {
        for (size_t j = i + 1; j < bindings.size(); ++j) {
            auto const & a = bindings[i];
            auto const & b = bindings[j];

            if (a.key == b.key && a.scope == b.scope) {
                conflicts.push_back({a.action_id, b.action_id, a.key, a.scope});
            }
        }
    }

    return conflicts;
}

// --- Scope resolution ---

std::optional<QString> KeymapManager::resolveKeyPress(
        QKeySequence const & key,
        EditorLib::EditorTypeId const & focused_type_id) const {

    if (key.isEmpty()) {
        return std::nullopt;
    }

    // 1. EditorFocused actions for the focused editor type
    if (focused_type_id.isValid()) {
        for (auto const & [id, desc]: _actions) {
            if (desc.scope.kind != KeyActionScopeKind::EditorFocused) {
                continue;
            }
            if (desc.scope.editor_type_id != focused_type_id) {
                continue;
            }
            if (!isActionEnabled(id)) {
                continue;
            }
            if (bindingFor(id) == key) {
                return id;
            }
        }
    }

    // 2. AlwaysRouted actions
    for (auto const & [id, desc]: _actions) {
        if (desc.scope.kind != KeyActionScopeKind::AlwaysRouted) {
            continue;
        }
        if (!isActionEnabled(id)) {
            continue;
        }
        if (bindingFor(id) == key) {
            return id;
        }
    }

    // 3. Global actions
    for (auto const & [id, desc]: _actions) {
        if (desc.scope.kind != KeyActionScopeKind::Global) {
            continue;
        }
        if (!isActionEnabled(id)) {
            continue;
        }
        if (bindingFor(id) == key) {
            return id;
        }
    }

    return std::nullopt;
}

// --- Adapter management ---

void KeymapManager::registerAdapter(KeyActionAdapter * adapter) {
    assert(adapter != nullptr && "registerAdapter: adapter must not be null");

    // Avoid duplicates
    auto const it = std::find(_adapters.begin(), _adapters.end(), adapter);
    if (it != _adapters.end()) {
        return;
    }

    _adapters.push_back(adapter);

    // Auto-cleanup when the adapter (or its parent widget) is destroyed
    connect(adapter, &QObject::destroyed, this, &KeymapManager::_removeAdapter);
}

bool KeymapManager::dispatchAction(
        QString const & action_id,
        EditorLib::EditorTypeId const & target_type_id,
        EditorLib::EditorInstanceId const & target_instance_id) const {

    for (auto * adapter: _adapters) {
        // Match by type
        if (adapter->typeId() != target_type_id) {
            continue;
        }

        // If a specific instance is requested, match that too
        if (target_instance_id.isValid() && adapter->instanceId() != target_instance_id) {
            continue;
        }

        if (adapter->handleKeyAction(action_id)) {
            return true;
        }
    }
    return false;
}

// --- Serialization ---

std::vector<KeymapOverrideEntry> KeymapManager::exportOverrides() const {
    std::vector<KeymapOverrideEntry> entries;
    entries.reserve(_overrides.size());

    for (auto const & [action_id, state]: _overrides) {
        entries.push_back({action_id.toStdString(),
                           state.key_sequence.toString().toStdString(),
                           state.enabled});
    }
    return entries;
}

void KeymapManager::importOverrides(std::vector<KeymapOverrideEntry> const & overrides) {
    for (auto const & entry: overrides) {
        auto & state = _overrides[QString::fromStdString(entry.action_id)];
        state.key_sequence = QKeySequence::fromString(QString::fromStdString(entry.key_sequence));
        state.enabled = entry.enabled;
    }
    if (!overrides.empty()) {
        emit bindingsChanged();
    }
}

// --- Private ---

void KeymapManager::_removeAdapter(QObject * adapter) {
    auto const it = std::find(_adapters.begin(), _adapters.end(), adapter);
    if (it != _adapters.end()) {
        _adapters.erase(it);
    }
}

}// namespace KeymapSystem
