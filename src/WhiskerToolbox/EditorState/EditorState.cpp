#include "EditorState.hpp"

#include <QUuid>

EditorState::EditorState(QObject * parent)
    : QObject(parent),
      _instance_id(generateInstanceId()),
      _display_name("Untitled"),
      _is_dirty(false) {
}

QString EditorState::getDisplayName() const {
    return _display_name;
}

void EditorState::setDisplayName(QString const & name) {
    if (_display_name != name) {
        _display_name = name;
        markDirty();
        emit displayNameChanged(name);
    }
}

void EditorState::markClean() {
    if (_is_dirty) {
        _is_dirty = false;
        emit dirtyChanged(false);
    }
}

void EditorState::markDirty() {
    if (!_is_dirty) {
        _is_dirty = true;
        emit dirtyChanged(true);
    }
    emit stateChanged();
}

QString EditorState::generateInstanceId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
