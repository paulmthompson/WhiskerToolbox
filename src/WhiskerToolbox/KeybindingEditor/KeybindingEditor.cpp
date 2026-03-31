/**
 * @file KeybindingEditor.cpp
 * @brief Implementation of the KeybindingEditor widget
 */

#include "KeybindingEditor.hpp"

#include "KeySequenceRecorder.hpp"
#include "KeymapModel.hpp"

#include "KeymapSystem/KeymapManager.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>

KeybindingEditor::KeybindingEditor(std::shared_ptr<KeybindingEditorState> state,
                                   KeymapSystem::KeymapManager * keymap_manager,
                                   QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _keymap_manager(keymap_manager) {
    _buildUi();

    if (_keymap_manager) {
        connect(_keymap_manager, &KeymapSystem::KeymapManager::bindingsChanged,
                this, &KeybindingEditor::_onBindingsChanged);
    }

    _updateButtonStates();
}

// --- UI construction ---

void KeybindingEditor::_buildUi() {
    auto * main_layout = new QVBoxLayout(this);

    // --- Search bar ---
    auto * search_layout = new QHBoxLayout;
    auto * search_label = new QLabel(QStringLiteral("Search:"), this);
    _search_edit = new QLineEdit(this);
    _search_edit->setPlaceholderText(QStringLiteral("Filter actions by name..."));
    _search_edit->setClearButtonEnabled(true);
    search_layout->addWidget(search_label);
    search_layout->addWidget(_search_edit);
    main_layout->addLayout(search_layout);

    // --- Tree view ---
    _model = new KeymapModel(_keymap_manager, this);

    _proxy_model = new QSortFilterProxyModel(this);
    _proxy_model->setSourceModel(_model);
    _proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
    _proxy_model->setRecursiveFilteringEnabled(true);
    _proxy_model->setFilterKeyColumn(KeymapModel::ColName);

    _tree_view = new QTreeView(this);
    _tree_view->setModel(_proxy_model);
    _tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
    _tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    _tree_view->setAlternatingRowColors(true);
    _tree_view->setRootIsDecorated(true);
    _tree_view->setUniformRowHeights(true);
    _tree_view->setSortingEnabled(false);
    _tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _tree_view->expandAll();

    // Column sizing
    auto * header = _tree_view->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(KeymapModel::ColName, QHeaderView::Stretch);
    header->setSectionResizeMode(KeymapModel::ColBinding, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(KeymapModel::ColScope, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(KeymapModel::ColDefault, QHeaderView::ResizeToContents);

    main_layout->addWidget(_tree_view, 1);// stretch factor = 1

    // --- Button row ---
    auto * button_layout = new QHBoxLayout;

    _record_button = new KeySequenceRecorder(this);
    _record_button->setToolTip(QStringLiteral("Record a new key binding for the selected action"));

    _clear_button = new QPushButton(QStringLiteral("Clear"), this);
    _clear_button->setToolTip(QStringLiteral("Unbind the selected action (set to no key)"));

    _reset_button = new QPushButton(QStringLiteral("Reset"), this);
    _reset_button->setToolTip(QStringLiteral("Reset the selected action to its default binding"));

    _reset_all_button = new QPushButton(QStringLiteral("Reset All"), this);
    _reset_all_button->setToolTip(QStringLiteral("Reset all bindings to their defaults"));

    button_layout->addWidget(_record_button);
    button_layout->addWidget(_clear_button);
    button_layout->addWidget(_reset_button);
    button_layout->addStretch();
    button_layout->addWidget(_reset_all_button);
    main_layout->addLayout(button_layout);

    setLayout(main_layout);

    // --- Connections ---
    connect(_search_edit, &QLineEdit::textChanged,
            this, &KeybindingEditor::_onFilterTextChanged);

    connect(_tree_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](QItemSelection const &, QItemSelection const &) { _onSelectionChanged(); });

    connect(_record_button, &KeySequenceRecorder::keySequenceRecorded,
            this, [this](QKeySequence const & seq) { _onKeyRecorded(seq); });

    connect(_clear_button, &QPushButton::clicked,
            this, &KeybindingEditor::_onClearBindingClicked);

    connect(_reset_button, &QPushButton::clicked,
            this, &KeybindingEditor::_onResetClicked);

    connect(_reset_all_button, &QPushButton::clicked,
            this, &KeybindingEditor::_onResetAllClicked);
}

// --- Slots ---

void KeybindingEditor::_onSelectionChanged() {
    // Cancel any active recording when selection changes
    if (_record_button->isRecording()) {
        _record_button->cancelRecording();
    }
    _updateButtonStates();
}

void KeybindingEditor::_onRecordClicked() {
    _record_button->startRecording();
}

void KeybindingEditor::_onKeyRecorded(QKeySequence const & seq) {
    auto const indexes = _tree_view->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        return;
    }

    auto const source_index = _proxy_model->mapToSource(indexes.first());
    auto const action_id = _model->actionIdAt(source_index);
    if (action_id.isEmpty()) {
        return;
    }

    _keymap_manager->setUserOverride(action_id, seq);
}

void KeybindingEditor::_onResetClicked() {
    auto const indexes = _tree_view->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        return;
    }

    auto const source_index = _proxy_model->mapToSource(indexes.first());
    auto const action_id = _model->actionIdAt(source_index);
    if (action_id.isEmpty()) {
        return;
    }

    _keymap_manager->clearUserOverride(action_id);
}

void KeybindingEditor::_onResetAllClicked() {
    auto const result = QMessageBox::question(
            this,
            QStringLiteral("Reset All Bindings"),
            QStringLiteral("Reset all keybindings to their defaults?\n\n"
                           "This will remove all custom key assignments."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (result == QMessageBox::Yes) {
        _keymap_manager->clearAllOverrides();
    }
}

void KeybindingEditor::_onClearBindingClicked() {
    auto const indexes = _tree_view->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        return;
    }

    auto const source_index = _proxy_model->mapToSource(indexes.first());
    auto const action_id = _model->actionIdAt(source_index);
    if (action_id.isEmpty()) {
        return;
    }

    // Set an explicit empty override to unbind
    _keymap_manager->setUserOverride(action_id, QKeySequence{});
}

void KeybindingEditor::_onFilterTextChanged(QString const & text) {
    _proxy_model->setFilterFixedString(text);
    _tree_view->expandAll();
}

void KeybindingEditor::_onBindingsChanged() {
    _model->refresh();
    _tree_view->expandAll();
    _updateButtonStates();
}

// --- Private helpers ---

void KeybindingEditor::_updateButtonStates() {
    auto const indexes = _tree_view->selectionModel()->selectedRows();
    bool const has_action_selected = [&]() {
        if (indexes.isEmpty()) {
            return false;
        }
        auto const source_index = _proxy_model->mapToSource(indexes.first());
        auto const action_id = _model->actionIdAt(source_index);
        return !action_id.isEmpty();
    }();

    _record_button->setEnabled(has_action_selected);
    _clear_button->setEnabled(has_action_selected);
    _reset_button->setEnabled(has_action_selected);
}
