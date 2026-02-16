#ifndef STATE_MANAGER_HPP
#define STATE_MANAGER_HPP

/**
 * @file StateManager.hpp
 * @brief Central coordinator for application state persistence
 *
 * StateManager owns AppPreferences and SessionStore, providing a single
 * object for MainWindow to create and manage. It resolves the platform-
 * specific config directory via QStandardPaths and initializes sub-stores.
 *
 * ## Usage
 *
 * ```cpp
 * // In MainWindow constructor:
 * _state_manager = std::make_unique<StateManager>(this);
 * _state_manager->loadAll();
 *
 * // Access sub-stores:
 * auto * prefs = _state_manager->preferences();
 * auto * session = _state_manager->session();
 *
 * // In MainWindow::closeEvent:
 * _state_manager->saveAll();
 * ```
 *
 * @see AppPreferences for application-level preferences
 * @see SessionStore for session memory
 * @see STATE_MANAGEMENT_ROADMAP.md Phase 1
 */

#include <QObject>
#include <QString>

#include <memory>

namespace StateManagement {

class AppPreferences;
class SessionStore;
class WorkspaceManager;

class StateManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct StateManager with auto-detected config directory
     *
     * Uses QStandardPaths::AppConfigLocation to find the platform-appropriate
     * config directory (e.g., ~/.config/WhiskerToolbox on Linux).
     *
     * @param parent Parent QObject (typically MainWindow)
     */
    explicit StateManager(QObject * parent = nullptr);

    /**
     * @brief Construct StateManager with explicit config directory
     *
     * Useful for testing or custom installations.
     *
     * @param config_dir Directory to store config files
     * @param parent Parent QObject
     */
    explicit StateManager(QString const & config_dir, QObject * parent = nullptr);

    ~StateManager() override;

    // === Lifecycle ===

    /// Load all state stores from disk. Call once at startup.
    void loadAll();

    /// Save all state stores to disk immediately. Call at shutdown.
    void saveAll();

    // === Sub-store Access ===

    [[nodiscard]] AppPreferences * preferences() const { return _preferences.get(); }
    [[nodiscard]] SessionStore * session() const { return _session.get(); }
    [[nodiscard]] WorkspaceManager * workspace() const { return _workspace.get(); }

    // === Config Directory ===

    /// @return The config directory used by all sub-stores
    [[nodiscard]] QString configDir() const { return _config_dir; }

private:
    void _init(QString const & config_dir);

    QString _config_dir;
    std::unique_ptr<AppPreferences> _preferences;
    std::unique_ptr<SessionStore> _session;
    std::unique_ptr<WorkspaceManager> _workspace;
};

}// namespace StateManagement

#endif// STATE_MANAGER_HPP
