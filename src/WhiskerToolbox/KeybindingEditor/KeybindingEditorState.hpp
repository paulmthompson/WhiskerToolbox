/**
 * @file KeybindingEditorState.hpp
 * @brief EditorState subclass for the KeybindingEditor widget
 *
 * The KeybindingEditor has no persistent user-specific state beyond what
 * KeymapManager already stores in AppPreferencesData. This class satisfies
 * the EditorState contract with a trivial implementation.
 */

#ifndef KEYBINDING_EDITOR_STATE_HPP
#define KEYBINDING_EDITOR_STATE_HPP

#include "EditorState/EditorState.hpp"

/**
 * @brief Minimal EditorState for the KeybindingEditor widget.
 *
 * All persistent keybinding data is managed by KeymapManager via
 * AppPreferencesData. This state class exists only to satisfy the
 * EditorRegistry type system.
 */
class KeybindingEditorState : public EditorState {
    Q_OBJECT

public:
    explicit KeybindingEditorState(QObject * parent = nullptr);
    ~KeybindingEditorState() override = default;

    [[nodiscard]] QString getTypeName() const override {
        return QStringLiteral("KeybindingEditor");
    }

    [[nodiscard]] std::string toJson() const override { return "{}"; }
    bool fromJson(std::string const & /*json*/) override { return true; }
};

#endif// KEYBINDING_EDITOR_STATE_HPP
