/**
 * @file KeybindingEditor.cpp
 * @brief Implementation of the KeybindingEditor widget
 */

#include "KeybindingEditor.hpp"

#include <QLabel>
#include <QVBoxLayout>

KeybindingEditor::KeybindingEditor(std::shared_ptr<KeybindingEditorState> state,
                                   KeymapSystem::KeymapManager * keymap_manager,
                                   QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _keymap_manager(keymap_manager) {
    auto * layout = new QVBoxLayout(this);
    auto * placeholder = new QLabel(QStringLiteral("Keybinding Editor — coming soon"), this);
    layout->addWidget(placeholder);
    layout->addStretch();
    setLayout(layout);
}
