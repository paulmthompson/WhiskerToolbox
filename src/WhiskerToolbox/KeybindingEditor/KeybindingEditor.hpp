/**
 * @file KeybindingEditor.hpp
 * @brief Widget for viewing and editing configurable keyboard shortcuts
 *
 * Displays all registered KeyActionDescriptors grouped by category in a
 * QTreeView, with controls for recording new bindings, resetting to defaults,
 * and filtering by search text.
 *
 * The actual binding data is owned by KeymapManager — this widget is a pure
 * view/controller on top of it.
 *
 * @see KeymapManager for the binding registry and override management
 * @see KeymapModel for the tree data model
 * @see KeySequenceRecorder for key capture
 * @see KeybindingEditorRegistration for EditorRegistry integration
 */

#ifndef KEYBINDING_EDITOR_HPP
#define KEYBINDING_EDITOR_HPP

#include <QPushButton>
#include <QWidget>

class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;

namespace KeymapSystem {
class KeymapManager;
}// namespace KeymapSystem

class KeybindingEditorState;
class KeymapModel;
class KeySequenceRecorder;

/**
 * @brief Panel for viewing and editing keybindings
 *
 * Layout:
 * - Search bar (top)
 * - QTreeView showing Category → Action tree (center)
 * - Record / Reset / Reset All controls (bottom)
 */
class KeybindingEditor : public QWidget {
    Q_OBJECT

public:
    explicit KeybindingEditor(std::shared_ptr<KeybindingEditorState> state,
                              KeymapSystem::KeymapManager * keymap_manager,
                              QWidget * parent = nullptr);
    ~KeybindingEditor() override = default;

private slots:
    void _onSelectionChanged();
    void _onRecordClicked();
    void _onKeyRecorded(QKeySequence const & seq);
    void _onResetClicked();
    void _onResetAllClicked();
    void _onClearBindingClicked();
    void _onFilterTextChanged(QString const & text);
    void _onBindingsChanged();

private:
    void _buildUi();
    void _updateButtonStates();

    std::shared_ptr<KeybindingEditorState> _state;
    KeymapSystem::KeymapManager * _keymap_manager = nullptr;

    // UI components
    QLineEdit * _search_edit = nullptr;
    QTreeView * _tree_view = nullptr;
    KeymapModel * _model = nullptr;
    QSortFilterProxyModel * _proxy_model = nullptr;
    KeySequenceRecorder * _record_button = nullptr;
    QPushButton * _reset_button = nullptr;
    QPushButton * _clear_button = nullptr;
    QPushButton * _reset_all_button = nullptr;
};

#endif// KEYBINDING_EDITOR_HPP
