/**
 * @file KeymapManager.cpp
 * @brief Implementation of the central keymap manager
 */

#include "KeymapSystem/KeymapManager.hpp"

#include "KeymapSystem/KeyActionAdapter.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"
#include "EditorState/SelectionContext.hpp"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>

namespace KeymapSystem {

KeymapManager::KeymapManager(QObject * parent)
    : QObject(parent) {}

KeymapManager::~KeymapManager() = default;

// --- Event filter ---

bool KeymapManager::eventFilter(QObject * obj, QEvent * event) {
    if (event->type() != QEvent::KeyPress) {
        return QObject::eventFilter(obj, event);
    }

    QWidget * focused = QApplication::focusWidget();
    bool const in_text_input = _isTextInputWidget(focused);

    auto * key_event = dynamic_cast<QKeyEvent *>(event);

    // Build a QKeySequence from the key event (key + modifiers)
    int const key_with_modifiers = key_event->key() | static_cast<int>(key_event->modifiers());
    QKeySequence const key_seq(key_with_modifiers);

    spdlog::debug("[KeymapManager] KeyPress: seq='{}' focused_widget='{}' in_text_input={}",
                  key_seq.toString().toStdString(),
                  focused ? focused->metaObject()->className() : "(none)",
                  in_text_input);

    // Determine the focused editor type from SelectionContext
    EditorLib::EditorTypeId focused_type_id;
    if (_editor_registry) {
        auto * context = _editor_registry->selectionContext();
        auto const instance_id = context->activeEditorId();
        if (instance_id.isValid()) {
            auto const state = _editor_registry->state(
                    EditorLib::EditorInstanceId(instance_id.value));
            if (state) {
                focused_type_id = EditorLib::EditorTypeId(state->getTypeName());
            }
        }
    }

    spdlog::debug("[KeymapManager] Focused editor type: '{}'",
                  focused_type_id.value.toStdString());

    // Resolve the key press through scope priority
    auto const action_id = resolveKeyPress(key_seq, focused_type_id);
    if (!action_id.has_value()) {
        spdlog::debug("[KeymapManager] No action bound to this key — passing through");
        return false;// No match — let the event pass through
    }
    spdlog::debug("[KeymapManager] Resolved to action: '{}'", action_id->toStdString());

    // Look up the action descriptor for target routing
    auto const desc = action(*action_id);
    if (!desc.has_value()) {
        return false;
    }

    // EditorFocused actions defer to text-input widgets so that typing
    // in QLineEdits, QComboBoxes, QSpinBoxes, etc. is not stolen.
    // AlwaysRouted and Global actions bypass this — they are meant to
    // fire regardless of where focus happens to be.
    if (in_text_input && desc->scope.kind == KeyActionScopeKind::EditorFocused) {
        spdlog::debug("[KeymapManager] Blocked: EditorFocused action '{}' deferred to text input",
                      action_id->toStdString());
        return false;
    }

    // Determine target type for dispatch
    EditorLib::EditorTypeId target_type_id;
    EditorLib::EditorInstanceId target_instance_id;
    switch (desc->scope.kind) {
        case KeyActionScopeKind::EditorFocused:
            target_type_id = focused_type_id;
            break;
        case KeyActionScopeKind::AlwaysRouted:
            target_type_id = desc->scope.editor_type_id;
            // Use active target if one is set for this type
            target_instance_id = activeTarget(target_type_id);
            break;
        case KeyActionScopeKind::Global:
            // Global actions: try dispatching to any adapter that handles it
            target_type_id = {};
            break;
    }

    spdlog::debug("[KeymapManager] Dispatching: action='{}' type='{}' instance='{}' adapters={}",
                  action_id->toStdString(),
                  target_type_id.value.toStdString(),
                  target_instance_id.value.toStdString(),
                  _adapters.size());

    // Dispatch to the appropriate adapter
    if (dispatchAction(*action_id, target_type_id, target_instance_id)) {
        spdlog::debug("[KeymapManager] Dispatch succeeded");
        return true;// Event consumed
    }

    spdlog::debug("[KeymapManager] Dispatch failed — no adapter handled action '{}'",
                  action_id->toStdString());
    // Action resolved but no adapter handled it — don't consume the event
    return false;
}

// --- Editor registry integration ---

void KeymapManager::setEditorRegistry(EditorRegistry * registry) {
    _editor_registry = registry;
}

// --- Text-input bypass ---

bool KeymapManager::_isTextInputWidget(QWidget * widget) {
    if (!widget) {
        return false;
    }
    return qobject_cast<QLineEdit *>(widget) != nullptr ||
           qobject_cast<QTextEdit *>(widget) != nullptr ||
           qobject_cast<QPlainTextEdit *>(widget) != nullptr ||
           qobject_cast<QComboBox *>(widget) != nullptr ||
           qobject_cast<QAbstractSpinBox *>(widget) != nullptr;
}

// --- Registration ---

bool KeymapManager::registerAction(KeyActionDescriptor const & descriptor) {
    assert(!descriptor.action_id.isEmpty() && "registerAction: action_id must not be empty");

    // Precondition #2: AlwaysRouted and EditorFocused scopes require a non-empty editor_type_id.
    // An empty type_id silently routes dispatch to the wrong adapters.
    assert((descriptor.scope.kind == KeyActionScopeKind::Global ||
            !descriptor.scope.editor_type_id.value.isEmpty()) &&
           "registerAction: AlwaysRouted/EditorFocused scope requires a non-empty editor_type_id");

    if (_actions.contains(descriptor.action_id)) {
        spdlog::debug("[KeymapManager] registerAction: duplicate action_id '{}' — ignored",
                      descriptor.action_id.toStdString());
        return false;
    }
    _actions.emplace(descriptor.action_id, descriptor);
    spdlog::debug("[KeymapManager] Registered action '{}' scope={}",
                  descriptor.action_id.toStdString(),
                  static_cast<int>(descriptor.scope.kind));
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

    spdlog::debug("[KeymapManager::dispatch] action='{}' target_type='{}' target_instance='{}' total_adapters={}",
                  action_id.toStdString(),
                  target_type_id.value.toStdString(),
                  target_instance_id.value.toStdString(),
                  _adapters.size());

    for (auto * adapter: _adapters) {
        // Match by type
        if (adapter->typeId() != target_type_id) {
            spdlog::debug("[KeymapManager::dispatch] Skip adapter: type mismatch ('{}' != '{}')",
                          adapter->typeId().value.toStdString(),
                          target_type_id.value.toStdString());
            continue;
        }

        // If a specific instance is requested, match that too
        if (target_instance_id.isValid() && adapter->instanceId() != target_instance_id) {
            spdlog::debug("[KeymapManager::dispatch] Skip adapter: instance mismatch ('{}' != '{}')",
                          adapter->instanceId().value.toStdString(),
                          target_instance_id.value.toStdString());
            continue;
        }

        spdlog::debug("[KeymapManager::dispatch] Trying adapter type='{}' instance='{}'",
                      adapter->typeId().value.toStdString(),
                      adapter->instanceId().value.toStdString());

        if (adapter->handleKeyAction(action_id)) {
            spdlog::debug("[KeymapManager::dispatch] Handled by adapter instance='{}'",
                          adapter->instanceId().value.toStdString());
            return true;
        }
        spdlog::debug("[KeymapManager::dispatch] Adapter declined action");
    }
    spdlog::debug("[KeymapManager::dispatch] No adapter handled the action");
    return false;
}

// --- Active target management ---

void KeymapManager::cycleActiveTarget(EditorLib::EditorTypeId const & type_id) {
    // Collect all adapters matching this type
    std::vector<KeyActionAdapter *> matching;
    for (auto * adapter: _adapters) {
        if (adapter->typeId() == type_id) {
            matching.push_back(adapter);
        }
    }

    if (matching.empty()) {
        return;
    }

    // Find the current active target
    auto const current_it = _active_targets.find(type_id);
    EditorLib::EditorInstanceId next_id;

    if (current_it == _active_targets.end() || !current_it->second.isValid()) {
        // No active target — select first
        next_id = matching.front()->instanceId();
    } else {
        // Find the current adapter in the list and advance to next
        auto const pos = std::find_if(matching.begin(), matching.end(),
                                      [&](KeyActionAdapter const * a) {
                                          return a->instanceId() == current_it->second;
                                      });
        if (pos == matching.end() || std::next(pos) == matching.end()) {
            // Current not found or at end — wrap to first
            next_id = matching.front()->instanceId();
        } else {
            next_id = (*std::next(pos))->instanceId();
        }
    }

    _active_targets[type_id] = next_id;
    emit activeTargetChanged(type_id, next_id);
}

void KeymapManager::setActiveTarget(EditorLib::EditorTypeId const & type_id,
                                    EditorLib::EditorInstanceId const & instance_id) {
    auto const old_it = _active_targets.find(type_id);
    if (old_it != _active_targets.end() && old_it->second == instance_id) {
        return;// No change
    }
    if (instance_id.isValid()) {
        _active_targets[type_id] = instance_id;
    } else {
        _active_targets.erase(type_id);
    }
    emit activeTargetChanged(type_id, instance_id);
}

EditorLib::EditorInstanceId KeymapManager::activeTarget(
        EditorLib::EditorTypeId const & type_id) const {
    auto const it = _active_targets.find(type_id);
    if (it != _active_targets.end()) {
        return it->second;
    }
    return {};
}

std::vector<EditorLib::EditorInstanceId> KeymapManager::adapterInstancesForType(
        EditorLib::EditorTypeId const & type_id) const {
    std::vector<EditorLib::EditorInstanceId> result;
    for (auto const * adapter: _adapters) {
        if (adapter->typeId() == type_id) {
            result.push_back(adapter->instanceId());
        }
    }
    return result;
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
        auto const * typed = static_cast<KeyActionAdapter const *>(*it);
        auto const type_id = typed->typeId();
        auto const instance_id = typed->instanceId();

        _adapters.erase(it);

        // If the removed adapter was the active target for its type, clear it
        auto const target_it = _active_targets.find(type_id);
        if (target_it != _active_targets.end() && target_it->second == instance_id) {
            _active_targets.erase(target_it);
            emit activeTargetChanged(type_id, {});
        }
    }
}

}// namespace KeymapSystem
