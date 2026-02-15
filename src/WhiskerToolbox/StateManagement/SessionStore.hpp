#ifndef SESSION_STORE_HPP
#define SESSION_STORE_HPP

/**
 * @file SessionStore.hpp
 * @brief Qt wrapper for session memory with auto-save
 *
 * SessionStore tracks transient cross-session state: last-used file paths
 * for each dialog, recent workspace list, and window geometry.
 *
 * ## Usage
 *
 * ```cpp
 * auto session = std::make_unique<SessionStore>(config_dir);
 * session->load();
 *
 * // Remember where the user last opened a file for a specific dialog
 * session->rememberPath("import_csv", "/home/user/data");
 *
 * // Next time that dialog opens, start there
 * auto dir = session->lastUsedPath("import_csv", QDir::homePath());
 *
 * // Window geometry
 * session->restoreWindowGeometry(mainWindow);
 * // ... at shutdown:
 * session->captureWindowGeometry(mainWindow);
 * session->save();
 * ```
 *
 * @see SessionData for the serializable data structure
 * @see StateManager for the owning coordinator
 */

#include "SessionData.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

class QMainWindow;
class QTimer;

namespace StateManagement {

class SessionStore : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct SessionStore
     * @param config_dir Directory where session.json will be stored
     * @param parent Parent QObject
     */
    explicit SessionStore(QString const & config_dir, QObject * parent = nullptr);
    ~SessionStore() override;

    // === Persistence ===

    /// Load session data from disk. Called once at startup.
    void load();

    /// Save session data to disk immediately. Called at shutdown and by debounce timer.
    void save();

    /// @return Absolute path to the session JSON file
    [[nodiscard]] QString filePath() const { return _file_path; }

    // === Per-Dialog Path Memory ===

    /**
     * @brief Get the last-used path for a specific dialog
     * @param dialog_id Unique identifier for the dialog (e.g., "import_csv", "load_video")
     * @param fallback Path to return if no memory exists for this dialog
     * @return The last-used path, or fallback if none recorded
     */
    [[nodiscard]] QString lastUsedPath(QString const & dialog_id,
                                        QString const & fallback = {}) const;

    /**
     * @brief Remember a path for a specific dialog
     *
     * If the path is a file, the parent directory is stored instead.
     * This auto-saves after a debounce period.
     *
     * @param dialog_id Unique identifier for the dialog
     * @param path File or directory path to remember
     */
    void rememberPath(QString const & dialog_id, QString const & path);

    // === Recent Workspaces ===

    [[nodiscard]] QStringList recentWorkspaces() const;

    /**
     * @brief Add a workspace to the recent list
     *
     * Moves the path to the front if already present.
     * Caps the list at SessionData::max_recent_workspaces.
     */
    void addRecentWorkspace(QString const & path);

    /// Remove a workspace from the recent list (e.g., file no longer exists)
    void removeRecentWorkspace(QString const & path);

    // === Window Geometry ===

    /**
     * @brief Capture the current window geometry into session data
     * @param window The main window to capture from
     */
    void captureWindowGeometry(QMainWindow const * window);

    /**
     * @brief Restore window geometry from session data
     * @param window The main window to restore to
     */
    void restoreWindowGeometry(QMainWindow * window) const;

    // === Direct access (for testing / advanced use) ===

    [[nodiscard]] SessionData const & data() const { return _data; }

signals:
    /// Emitted when session data changes
    void sessionChanged();

private:
    void _scheduleSave();

    SessionData _data;
    QString _file_path;
    QTimer * _save_timer = nullptr;

    static constexpr int SaveDebounceMs = 500;
};

} // namespace StateManagement

#endif // SESSION_STORE_HPP
