/**
 * @file KeybindingEditor.hpp
 * @brief Widget for viewing and editing configurable keyboard shortcuts
 *
 * Displays all registered KeyActionDescriptors grouped by category, with
 * controls for recording new bindings, resetting to defaults, and searching.
 * The actual binding data is owned by KeymapManager.
 *
 * @see KeymapManager for the binding registry and override management
 * @see KeybindingEditorRegistration for EditorRegistry integration
 */

#ifndef KEYBINDING_EDITOR_HPP
#define KEYBINDING_EDITOR_HPP

#include <QWidget>

namespace KeymapSystem {
class KeymapManager;
}// namespace KeymapSystem

class KeybindingEditorState;

/**
 * @brief Dock widget for viewing and editing keybindings.
 *
 * Placeholder implementation — UI will be populated in a later step.
 * The widget receives a non-owning pointer to KeymapManager at
 * construction time; KeymapManager is guaranteed to outlive this widget.
 */
class KeybindingEditor : public QWidget {
    Q_OBJECT

public:
    explicit KeybindingEditor(std::shared_ptr<KeybindingEditorState> state,
                              KeymapSystem::KeymapManager * keymap_manager,
                              QWidget * parent = nullptr);
    ~KeybindingEditor() override = default;

private:
    std::shared_ptr<KeybindingEditorState> _state;
    KeymapSystem::KeymapManager * _keymap_manager = nullptr;
};

#endif// KEYBINDING_EDITOR_HPP
