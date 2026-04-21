/**
 * @file TriageSessionProperties_Widget.hpp
 * @brief Properties widget for the Triage Session
 *
 * Provides Mark/Commit/Recall buttons, pipeline display (load/edit JSON,
 * command summary), and tracked-regions summary (interval count, frame count).
 */

#ifndef TRIAGE_SESSION_PROPERTIES_WIDGET_HPP
#define TRIAGE_SESSION_PROPERTIES_WIDGET_HPP

#include "TimeFrame/TimeFrameIndex.hpp"

#include <QWidget>

#include <memory>
#include <optional>

class EditorRegistry;
class GuidedPipelineEditor;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class TriageSessionState;

namespace commands {
class CommandRecorder;
struct CommandSequenceDescriptor;
}// namespace commands

namespace KeymapSystem {
class KeyActionAdapter;
class KeymapManager;
}// namespace KeymapSystem

namespace triage {
class TriageSession;
}

struct TimePosition;

/**
 * @brief Properties panel for the Triage Session widget
 *
 * This is a properties-only widget (no separate view panel).
 * Provides:
 * - Mark / Commit / Recall buttons with state-dependent enable/disable
 * - Status display showing current state and mark frame
 * - Pipeline display: load JSON, edit JSON, show command summary
 * - Tracked-regions summary: interval count, frame count
 */
class TriageSessionProperties_Widget : public QWidget {
    Q_OBJECT

public:
    explicit TriageSessionProperties_Widget(std::shared_ptr<TriageSessionState> state,
                                            EditorRegistry * editor_registry,
                                            QWidget * parent = nullptr);
    ~TriageSessionProperties_Widget() override;

    /**
     * @brief Set the CommandRecorder for recording command executions
     * @param recorder Non-owning pointer to the CommandRecorder (can be nullptr)
     */
    void setCommandRecorder(commands::CommandRecorder * recorder) { _command_recorder = recorder; }

    /**
     * @brief Connect this widget to the KeymapManager for keymap-driven actions
     * @param manager Non-owning pointer (can be nullptr)
     */
    void setKeymapManager(KeymapSystem::KeymapManager * manager);

private slots:
    void _onMarkClicked();
    void _onCommitClicked();
    void _onRecallClicked();
    void _onLoadPipelineClicked();
    void _onPipelineTextChanged();
    void _onGuidedEditorChanged();
    void _onTimeChanged(TimePosition const & position);

private:
    void _buildUI();
    void _connectSignals();
    void _updateStateDisplay();
    void _updateButtonStates();
    void _updateCommandSummary();
    void _updateTrackedRegionsSummary();
    void _syncPipelineFromState();
    bool _handleSequenceSlotAction(QString const & action_id);
    static std::optional<int> _parseSequenceSlotIndex(QString const & action_id);
    [[nodiscard]] QString _selectedSlotActionId() const;
    void _syncSlotEditorFromState();
    void _refreshSlotSelectorLabels();
    void _updateSlotBindingDisplay();
    void _onSlotSelectionChanged(int index);
    void _onSlotDisplayNameChanged(QString const & name);
    void _onSlotEnabledChanged(bool enabled);
    void _onCopyPipelineClicked();
    void _onExportPipelineClicked();
    void _clearValidationBanner();
    void _updateValidationBanner(commands::CommandSequenceDescriptor const & seq);

    std::shared_ptr<TriageSessionState> _state;
    EditorRegistry * _editor_registry = nullptr;
    std::unique_ptr<triage::TriageSession> _session;
    TimeFrameIndex _current_frame{0};
    int _selected_slot_index = 1;

    // Status
    QLabel * _status_label = nullptr;

    // Buttons
    QPushButton * _mark_button = nullptr;
    QPushButton * _commit_button = nullptr;
    QPushButton * _recall_button = nullptr;

    // Pipeline
    QComboBox * _slot_selector = nullptr;
    QLineEdit * _slot_name_edit = nullptr;
    QCheckBox * _slot_enabled_checkbox = nullptr;
    QLabel * _slot_action_id_label = nullptr;
    QLabel * _slot_binding_label = nullptr;
    QPushButton * _load_pipeline_button = nullptr;
    QPushButton * _copy_pipeline_button = nullptr;
    QPushButton * _export_pipeline_button = nullptr;
    GuidedPipelineEditor * _guided_editor = nullptr;
    QGroupBox * _json_group = nullptr;
    QTextEdit * _pipeline_text_edit = nullptr;
    QLabel * _pipeline_name_label = nullptr;
    QLabel * _pipeline_validation_label = nullptr;

    // Tracked regions
    QLabel * _tracked_summary_label = nullptr;

    commands::CommandRecorder * _command_recorder{nullptr};

    KeymapSystem::KeymapManager * _keymap_manager = nullptr;
    KeymapSystem::KeyActionAdapter * _key_adapter{nullptr};

    bool _syncing_slot_controls = false;///< Guard against re-entrant slot metadata sync
    bool _syncing_from_editor = false;  ///< Guard against re-entrant pipeline sync
};

#endif// TRIAGE_SESSION_PROPERTIES_WIDGET_HPP
