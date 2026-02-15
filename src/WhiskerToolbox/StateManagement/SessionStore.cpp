#include "SessionStore.hpp"

#include <rfl/json.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QTimer>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace StateManagement {

SessionStore::SessionStore(QString const & config_dir, QObject * parent)
    : QObject(parent) {
    _file_path = config_dir + QStringLiteral("/session.json");

    _save_timer = new QTimer(this);
    _save_timer->setSingleShot(true);
    _save_timer->setInterval(SaveDebounceMs);
    connect(_save_timer, &QTimer::timeout, this, &SessionStore::save);
}

SessionStore::~SessionStore() {
    // Flush any pending save
    if (_save_timer->isActive()) {
        _save_timer->stop();
        save();
    }
}

void SessionStore::load() {
    QFile file(_file_path);
    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "SessionStore: failed to open " << _file_path.toStdString() << std::endl;
        return;
    }

    auto const json_str = QString(file.readAll()).toStdString();
    file.close();

    auto result = rfl::json::read<SessionData>(json_str);
    if (result) {
        _data = std::move(*result);
    } else {
        std::cerr << "SessionStore: failed to parse " << _file_path.toStdString() << std::endl;
    }
}

void SessionStore::save() {
    _save_timer->stop();

    QDir dir;
    dir.mkpath(QFileInfo(_file_path).absolutePath());

    std::ofstream file(_file_path.toStdString());
    if (!file.is_open()) {
        std::cerr << "SessionStore: failed to write " << _file_path.toStdString() << std::endl;
        return;
    }

    file << rfl::json::write(_data);
}

void SessionStore::_scheduleSave() {
    _save_timer->start();
}

// === Per-Dialog Path Memory ===

QString SessionStore::lastUsedPath(QString const & dialog_id,
                                    QString const & fallback) const {
    auto const key = dialog_id.toStdString();
    auto it = _data.last_used_paths.find(key);
    if (it != _data.last_used_paths.end() && !it->second.empty()) {
        auto const remembered = QString::fromStdString(it->second);
        // Only return if the directory still exists
        if (QDir(remembered).exists()) {
            return remembered;
        }
    }
    return fallback;
}

void SessionStore::rememberPath(QString const & dialog_id, QString const & path) {
    if (path.isEmpty()) {
        return;
    }

    // If path is a file, store the parent directory
    QFileInfo info(path);
    auto const dir_path = info.isDir() ? path : info.absolutePath();

    auto const key = dialog_id.toStdString();
    auto const value = dir_path.toStdString();

    if (_data.last_used_paths[key] != value) {
        _data.last_used_paths[key] = value;
        _scheduleSave();
        emit sessionChanged();
    }
}

// === Recent Workspaces ===

QStringList SessionStore::recentWorkspaces() const {
    QStringList result;
    result.reserve(static_cast<int>(_data.recent_workspaces.size()));
    for (auto const & ws : _data.recent_workspaces) {
        result.append(QString::fromStdString(ws));
    }
    return result;
}

void SessionStore::addRecentWorkspace(QString const & path) {
    if (path.isEmpty()) {
        return;
    }

    auto const path_str = path.toStdString();

    // Remove if already present (we'll re-add at front)
    auto & rw = _data.recent_workspaces;
    rw.erase(std::remove(rw.begin(), rw.end(), path_str), rw.end());

    // Insert at front
    rw.insert(rw.begin(), path_str);

    // Cap the list
    if (static_cast<int>(rw.size()) > SessionData::max_recent_workspaces) {
        rw.resize(SessionData::max_recent_workspaces);
    }

    _scheduleSave();
    emit sessionChanged();
}

void SessionStore::removeRecentWorkspace(QString const & path) {
    auto const path_str = path.toStdString();
    auto & rw = _data.recent_workspaces;
    auto const old_size = rw.size();
    rw.erase(std::remove(rw.begin(), rw.end(), path_str), rw.end());

    if (rw.size() != old_size) {
        _scheduleSave();
        emit sessionChanged();
    }
}

// === Window Geometry ===

void SessionStore::captureWindowGeometry(QMainWindow const * window) {
    if (!window) {
        return;
    }

    _data.window_maximized = window->isMaximized();

    if (!window->isMaximized()) {
        auto const geom = window->geometry();
        _data.window_x = geom.x();
        _data.window_y = geom.y();
        _data.window_width = geom.width();
        _data.window_height = geom.height();
    }

    _scheduleSave();
}

void SessionStore::restoreWindowGeometry(QMainWindow * window) const {
    if (!window) {
        return;
    }

    if (_data.window_maximized) {
        window->showMaximized();
    } else {
        window->setGeometry(_data.window_x, _data.window_y,
                            _data.window_width, _data.window_height);
    }
}

} // namespace StateManagement
