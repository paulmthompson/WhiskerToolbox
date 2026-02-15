#include "StateManager.hpp"

#include "AppPreferences.hpp"
#include "SessionStore.hpp"
#include "WorkspaceManager.hpp"

#include <QDir>
#include <QStandardPaths>

#include <iostream>

namespace StateManagement {

StateManager::StateManager(QObject * parent)
    : QObject(parent) {
    auto const config_dir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/WhiskerToolbox");
    _init(config_dir);
}

StateManager::StateManager(QString const & config_dir, QObject * parent)
    : QObject(parent) {
    _init(config_dir);
}

StateManager::~StateManager() {
    // Sub-stores flush pending saves in their destructors
}

void StateManager::_init(QString const & config_dir) {
    _config_dir = config_dir;

    // Ensure the config directory exists
    QDir dir;
    if (!dir.mkpath(_config_dir)) {
        std::cerr << "StateManager: failed to create config directory: "
                  << _config_dir.toStdString() << std::endl;
    }

    _preferences = std::make_unique<AppPreferences>(_config_dir, this);
    _session = std::make_unique<SessionStore>(_config_dir, this);
    _workspace = std::make_unique<WorkspaceManager>(_config_dir, this);
}

void StateManager::loadAll() {
    _preferences->load();
    _session->load();
}

void StateManager::saveAll() {
    _preferences->save();
    _session->save();
}

} // namespace StateManagement
