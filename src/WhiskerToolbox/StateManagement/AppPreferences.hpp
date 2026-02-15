#ifndef APP_PREFERENCES_HPP
#define APP_PREFERENCES_HPP

/**
 * @file AppPreferences.hpp
 * @brief Qt wrapper for application-level preferences with auto-save
 *
 * AppPreferences provides typed accessors over AppPreferencesData,
 * with Qt signal emission on changes and debounced auto-save to disk.
 *
 * ## Usage
 *
 * ```cpp
 * auto prefs = std::make_unique<AppPreferences>(config_dir);
 * prefs->load();
 *
 * // Read
 * auto paths = prefs->pythonEnvSearchPaths();
 *
 * // Write (auto-saved after debounce period)
 * prefs->setPythonEnvSearchPaths({"/usr/bin", "/home/user/.venvs"});
 *
 * // Force save (e.g., at shutdown)
 * prefs->save();
 * ```
 *
 * @see AppPreferencesData for the serializable data structure
 * @see StateManager for the owning coordinator
 */

#include "AppPreferencesData.hpp"

#include <QObject>
#include <QString>

#include <string>
#include <vector>

class QTimer;

namespace StateManagement {

class AppPreferences : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct AppPreferences
     * @param config_dir Directory where preferences.json will be stored
     * @param parent Parent QObject
     */
    explicit AppPreferences(QString const & config_dir, QObject * parent = nullptr);
    ~AppPreferences() override;

    // === Persistence ===

    /// Load preferences from disk. Called once at startup.
    void load();

    /// Save preferences to disk immediately. Called at shutdown and by debounce timer.
    void save();

    /// @return Absolute path to the preferences JSON file
    [[nodiscard]] QString filePath() const { return _file_path; }

    // === Python Environment ===

    [[nodiscard]] std::vector<std::string> pythonEnvSearchPaths() const;
    void setPythonEnvSearchPaths(std::vector<std::string> const & paths);

    [[nodiscard]] std::string preferredPythonEnv() const;
    void setPreferredPythonEnv(std::string const & path);

    // === File Dialogs ===

    [[nodiscard]] std::string defaultImportDirectory() const;
    void setDefaultImportDirectory(std::string const & path);

    [[nodiscard]] std::string defaultExportDirectory() const;
    void setDefaultExportDirectory(std::string const & path);

    // === Data Loading ===

    [[nodiscard]] std::string defaultTimeFrameKey() const;
    void setDefaultTimeFrameKey(std::string const & key);

    // === Direct access (for testing / advanced use) ===

    [[nodiscard]] AppPreferencesData const & data() const { return _data; }

signals:
    /// Emitted when any preference changes. Key identifies which field changed.
    void preferenceChanged(QString key);

private:
    void _scheduleSave();

    AppPreferencesData _data;
    QString _file_path;
    QTimer * _save_timer = nullptr;

    static constexpr int SaveDebounceMs = 500;
};

} // namespace StateManagement

#endif // APP_PREFERENCES_HPP
