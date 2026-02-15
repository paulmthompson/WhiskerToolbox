#ifndef WORKSPACE_MANAGER_HPP
#define WORKSPACE_MANAGER_HPP

/**
 * @file WorkspaceManager.hpp
 * @brief Workspace save/restore and crash recovery
 *
 * WorkspaceManager captures the current session (editor states, zone layout,
 * data load provenance) into a single `.wtb` JSON file and can restore
 * from that file.  It also provides auto-save to a recovery file so that
 * work survives crashes.
 *
 * ## Usage
 *
 * ```cpp
 * // Created by StateManager, collaborators wired up by MainWindow:
 * workspace->setEditorRegistry(registry);
 * workspace->setZoneManager(zone_manager);
 *
 * // Track data loads
 * workspace->recordDataLoad({"json_config", "/path/to/config.json"});
 *
 * // Save workspace
 * workspace->saveWorkspace("/path/to/experiment.wtb");
 *
 * // Enable crash-recovery auto-save (every 30 s)
 * workspace->enableAutoSave();
 *
 * // At startup, check for recovery
 * if (workspace->hasRecoveryFile()) { ... }
 * ```
 *
 * @see WorkspaceData for the serialized state structures
 * @see STATE_MANAGEMENT_ROADMAP.md Phase 2
 */

#include "WorkspaceData.hpp"

#include <QObject>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

class QTimer;
class EditorRegistry;
class ZoneManager;

namespace StateManagement {

class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    /// File extension for workspace files (including dot).
    static constexpr char const * kWorkspaceExtension = ".wtb";
    /// Default auto-save interval in milliseconds (30 seconds).
    static constexpr int kDefaultAutoSaveIntervalMs = 30000;

    /**
     * @brief Construct WorkspaceManager
     * @param config_dir Application config directory (recovery file lives here)
     * @param parent Parent QObject
     */
    explicit WorkspaceManager(QString const & config_dir, QObject * parent = nullptr);

    ~WorkspaceManager() override;

    // === Collaborators (set once after construction) ===

    void setEditorRegistry(EditorRegistry * registry);
    void setZoneManager(ZoneManager * zone_manager);

    // === Save / Load ===

    /**
     * @brief Save the current session to a workspace file
     *
     * Captures editor states, zone layout, and data load entries.
     * Updates the current workspace path and clears the dirty flag.
     *
     * @param file_path Absolute path to the workspace file
     * @return true on success
     */
    bool saveWorkspace(QString const & file_path);

    /**
     * @brief Load a workspace file and return its data
     *
     * Parses the workspace file and returns the data. The caller (MainWindow)
     * is responsible for orchestrating the actual restore (data reload,
     * state restore, widget recreation, layout application).
     *
     * @param file_path Path to the workspace file
     * @return WorkspaceData on success, std::nullopt on failure
     */
    [[nodiscard]] std::optional<WorkspaceData> readWorkspace(QString const & file_path) const;

    /**
     * @brief Mark the workspace as successfully restored
     *
     * Sets the current path and clears dirty state after a successful
     * restore orchestrated by the caller.
     *
     * @param file_path The path of the workspace that was restored
     */
    void markRestored(QString const & file_path);

    // === Data Load Tracking ===

    /// Record a data-loading operation for workspace provenance.
    void recordDataLoad(DataLoadEntry entry);

    /// Record that a JSON config file was loaded.
    void recordJsonConfigLoad(QString const & config_file_path);

    /// Record that a video was loaded.
    void recordVideoLoad(QString const & video_path);

    /// Record that an image directory was loaded.
    void recordImagesLoad(QString const & images_dir);

    /// Clear all data load entries (e.g., before restoring a workspace).
    void clearDataLoads();

    /// Get the current list of data load entries.
    [[nodiscard]] std::vector<DataLoadEntry> const & dataLoads() const;

    // === Auto-Save (Crash Recovery) ===

    /**
     * @brief Enable periodic auto-save to the recovery file
     * @param interval_ms Interval in milliseconds (default 30 s)
     */
    void enableAutoSave(int interval_ms = kDefaultAutoSaveIntervalMs);

    /// Disable auto-save.
    void disableAutoSave();

    /// @return true if auto-save is currently enabled
    [[nodiscard]] bool isAutoSaveEnabled() const;

    /// @return true if a recovery file exists from a previous crash
    [[nodiscard]] bool hasRecoveryFile() const;

    /// Read the recovery file and return its data.
    [[nodiscard]] std::optional<WorkspaceData> readRecoveryFile() const;

    /// Delete the recovery file (after successful restore or user decline).
    void deleteRecoveryFile();

    // === State ===

    /// @return Path of the currently open workspace (empty if none)
    [[nodiscard]] QString currentWorkspacePath() const;

    /// @return true if the session has unsaved changes
    [[nodiscard]] bool hasUnsavedChanges() const;

    /// Mark the workspace as having unsaved changes.
    void markDirty();

    /// Mark the workspace as clean (matches the last save).
    void markClean();

    // === File Paths ===

    /// @return Absolute path to the recovery file
    [[nodiscard]] QString recoveryFilePath() const;

signals:
    /// Emitted after a successful save.
    void workspaceSaved(QString path);

    /// Emitted when the current workspace path changes.
    void workspacePathChanged(QString path);

    /// Emitted when the dirty flag changes.
    void dirtyChanged(bool has_unsaved);

private:
    /// Capture the current session into a WorkspaceData.
    [[nodiscard]] WorkspaceData _captureCurrentState() const;

    /// Write a WorkspaceData to a file as pretty-printed JSON.
    [[nodiscard]] bool _writeWorkspaceFile(QString const & path,
                                            WorkspaceData const & data) const;

    /// Read a workspace file and parse into WorkspaceData.
    [[nodiscard]] std::optional<WorkspaceData> _readWorkspaceFile(QString const & path) const;

    /// Called by the auto-save timer.
    void _performAutoSave();

    /// Emit dirtyChanged if the dirty flag actually changed.
    void _setDirty(bool dirty);

    // Config
    QString _config_dir;

    // Current state
    QString _current_path;
    bool _is_dirty = false;
    std::vector<DataLoadEntry> _data_loads;

    // Collaborators (non-owning)
    EditorRegistry * _editor_registry = nullptr;
    ZoneManager * _zone_manager = nullptr;

    // Auto-save
    QTimer * _auto_save_timer = nullptr;
};

} // namespace StateManagement

#endif // WORKSPACE_MANAGER_HPP
