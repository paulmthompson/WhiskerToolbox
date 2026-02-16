#include "AppPreferences.hpp"

#include <rfl/json.hpp>

#include <QDir>
#include <QFile>
#include <QTimer>

#include <fstream>
#include <iostream>

namespace StateManagement {

AppPreferences::AppPreferences(QString const & config_dir, QObject * parent)
    : QObject(parent) {
    _file_path = config_dir + QStringLiteral("/preferences.json");

    _save_timer = new QTimer(this);
    _save_timer->setSingleShot(true);
    _save_timer->setInterval(SaveDebounceMs);
    connect(_save_timer, &QTimer::timeout, this, &AppPreferences::save);
}

AppPreferences::~AppPreferences() {
    // Flush any pending save
    if (_save_timer->isActive()) {
        _save_timer->stop();
        save();
    }
}

void AppPreferences::load() {
    QFile file(_file_path);
    if (!file.exists()) {
        // First run — keep defaults
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "AppPreferences: failed to open " << _file_path.toStdString() << std::endl;
        return;
    }

    auto const json_str = QString(file.readAll()).toStdString();
    file.close();

    auto result = rfl::json::read<AppPreferencesData>(json_str);
    if (result) {
        _data = std::move(*result);
    } else {
        std::cerr << "AppPreferences: failed to parse " << _file_path.toStdString() << std::endl;
    }
}

void AppPreferences::save() {
    _save_timer->stop();

    // Ensure directory exists
    QDir dir;
    dir.mkpath(QFileInfo(_file_path).absolutePath());

    std::ofstream file(_file_path.toStdString());
    if (!file.is_open()) {
        std::cerr << "AppPreferences: failed to write " << _file_path.toStdString() << std::endl;
        return;
    }

    file << rfl::json::write(_data);
}

void AppPreferences::_scheduleSave() {
    _save_timer->start();
}

// === Python Environment ===

std::vector<std::string> AppPreferences::pythonEnvSearchPaths() const {
    return _data.python_env_search_paths;
}

void AppPreferences::setPythonEnvSearchPaths(std::vector<std::string> const & paths) {
    if (_data.python_env_search_paths != paths) {
        _data.python_env_search_paths = paths;
        _scheduleSave();
        emit preferenceChanged(QStringLiteral("python_env_search_paths"));
    }
}

std::string AppPreferences::preferredPythonEnv() const {
    return _data.preferred_python_env;
}

void AppPreferences::setPreferredPythonEnv(std::string const & path) {
    if (_data.preferred_python_env != path) {
        _data.preferred_python_env = path;
        _scheduleSave();
        emit preferenceChanged(QStringLiteral("preferred_python_env"));
    }
}

// === File Dialogs ===

std::string AppPreferences::defaultImportDirectory() const {
    return _data.default_import_directory;
}

void AppPreferences::setDefaultImportDirectory(std::string const & path) {
    if (_data.default_import_directory != path) {
        _data.default_import_directory = path;
        _scheduleSave();
        emit preferenceChanged(QStringLiteral("default_import_directory"));
    }
}

std::string AppPreferences::defaultExportDirectory() const {
    return _data.default_export_directory;
}

void AppPreferences::setDefaultExportDirectory(std::string const & path) {
    if (_data.default_export_directory != path) {
        _data.default_export_directory = path;
        _scheduleSave();
        emit preferenceChanged(QStringLiteral("default_export_directory"));
    }
}

// === Data Loading ===

std::string AppPreferences::defaultTimeFrameKey() const {
    return _data.default_time_frame_key;
}

void AppPreferences::setDefaultTimeFrameKey(std::string const & key) {
    if (_data.default_time_frame_key != key) {
        _data.default_time_frame_key = key;
        _scheduleSave();
        emit preferenceChanged(QStringLiteral("default_time_frame_key"));
    }
}

}// namespace StateManagement
