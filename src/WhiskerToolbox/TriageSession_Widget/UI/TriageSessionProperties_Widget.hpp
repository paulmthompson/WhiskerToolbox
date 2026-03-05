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

class EditorRegistry;
class QLabel;
class QPushButton;
class QTextEdit;
class TriageSessionState;

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

private slots:
    void _onMarkClicked();
    void _onCommitClicked();
    void _onRecallClicked();
    void _onLoadPipelineClicked();
    void _onPipelineTextChanged();
    void _onTimeChanged(TimePosition const & position);

private:
    void _buildUI();
    void _connectSignals();
    void _updateStateDisplay();
    void _updateButtonStates();
    void _updateCommandSummary();
    void _updateTrackedRegionsSummary();
    void _syncPipelineFromState();

    std::shared_ptr<TriageSessionState> _state;
    EditorRegistry * _editor_registry = nullptr;
    std::unique_ptr<triage::TriageSession> _session;
    TimeFrameIndex _current_frame{0};

    // Status
    QLabel * _status_label = nullptr;

    // Buttons
    QPushButton * _mark_button = nullptr;
    QPushButton * _commit_button = nullptr;
    QPushButton * _recall_button = nullptr;

    // Pipeline
    QPushButton * _load_pipeline_button = nullptr;
    QTextEdit * _pipeline_text_edit = nullptr;
    QLabel * _pipeline_name_label = nullptr;
    QLabel * _command_summary_label = nullptr;

    // Tracked regions
    QLabel * _tracked_summary_label = nullptr;
};

#endif// TRIAGE_SESSION_PROPERTIES_WIDGET_HPP
