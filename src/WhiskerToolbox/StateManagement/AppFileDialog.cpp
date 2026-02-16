#include "AppFileDialog.hpp"

#include "SessionStore.hpp"

#include <QDir>

namespace {

StateManagement::SessionStore * s_session = nullptr;

QString resolveInitialDir(QString const & dialog_id, QString const & fallback_dir) {
    if (s_session) {
        auto const stored = s_session->lastUsedPath(dialog_id);
        if (!stored.isEmpty()) {
            return stored;
        }
    }
    return fallback_dir.isEmpty() ? QDir::currentPath() : fallback_dir;
}

void rememberIfValid(QString const & dialog_id, QString const & path) {
    if (s_session && !path.isEmpty()) {
        s_session->rememberPath(dialog_id, path);
    }
}

}// namespace

namespace AppFileDialog {

void init(StateManagement::SessionStore * session) {
    s_session = session;
}

QString getOpenFileName(
        QWidget * parent,
        QString const & dialog_id,
        QString const & caption,
        QString const & filter,
        QString const & fallback_dir) {
    auto const dir = resolveInitialDir(dialog_id, fallback_dir);
    auto result = QFileDialog::getOpenFileName(parent, caption, dir, filter);
    rememberIfValid(dialog_id, result);
    return result;
}

QString getSaveFileName(
        QWidget * parent,
        QString const & dialog_id,
        QString const & caption,
        QString const & filter,
        QString const & fallback_dir,
        QString const & suggested_name) {
    auto const dir = resolveInitialDir(dialog_id, fallback_dir);
    auto const initial = suggested_name.isEmpty() ? dir : (dir + QStringLiteral("/") + suggested_name);
    auto result = QFileDialog::getSaveFileName(parent, caption, initial, filter);
    rememberIfValid(dialog_id, result);
    return result;
}

QString getExistingDirectory(
        QWidget * parent,
        QString const & dialog_id,
        QString const & caption,
        QString const & fallback_dir,
        QFileDialog::Options options) {
    auto const dir = resolveInitialDir(dialog_id, fallback_dir);
    auto result = QFileDialog::getExistingDirectory(parent, caption, dir, options);
    rememberIfValid(dialog_id, result);
    return result;
}

}// namespace AppFileDialog
