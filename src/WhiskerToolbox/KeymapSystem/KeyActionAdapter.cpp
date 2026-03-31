/**
 * @file KeyActionAdapter.cpp
 * @brief Implementation of the composition-based keymap dispatch component
 */

#include "KeymapSystem/KeyActionAdapter.hpp"

#include <utility>

namespace KeymapSystem {

KeyActionAdapter::KeyActionAdapter(QObject * parent)
    : QObject(parent) {}

void KeyActionAdapter::setHandler(Handler handler) {
    _handler = std::move(handler);
}

bool KeyActionAdapter::handleKeyAction(QString const & action_id) {
    if (_handler) {
        return _handler(action_id);
    }
    return false;
}

void KeyActionAdapter::setInstanceId(EditorLib::EditorInstanceId const & id) {
    _instance_id = id;
}

EditorLib::EditorInstanceId KeyActionAdapter::instanceId() const {
    return _instance_id;
}

void KeyActionAdapter::setTypeId(EditorLib::EditorTypeId const & id) {
    _type_id = id;
}

EditorLib::EditorTypeId KeyActionAdapter::typeId() const {
    return _type_id;
}

}// namespace KeymapSystem
