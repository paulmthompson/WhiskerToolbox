/**
 * @file KeybindingEditorRegistration.hpp
 * @brief Registration interface for the KeybindingEditor widget module
 */

#ifndef KEYBINDING_EDITOR_REGISTRATION_HPP
#define KEYBINDING_EDITOR_REGISTRATION_HPP

class EditorRegistry;

namespace KeymapSystem {
class KeymapManager;
}// namespace KeymapSystem

namespace KeybindingEditorModule {

/**
 * @brief Register the KeybindingEditor type with the EditorRegistry.
 *
 * @param registry      The application EditorRegistry. Must not be null.
 * @param keymap_manager Non-owning pointer to KeymapManager, forwarded to
 *                       the widget factory. May be null (widget becomes a
 *                       no-op placeholder in that case).
 */
void registerTypes(EditorRegistry * registry,
                   KeymapSystem::KeymapManager * keymap_manager);

}// namespace KeybindingEditorModule

#endif// KEYBINDING_EDITOR_REGISTRATION_HPP
