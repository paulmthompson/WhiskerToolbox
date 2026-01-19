#include "SelectionContext.hpp"

#include <algorithm>

SelectionContext::SelectionContext(QObject * parent)
    : QObject(parent) {
}

// === Data Selection ===

void SelectionContext::setSelectedData(QString const & data_key, SelectionSource const & source) {
    _selected_data.clear();
    _selected_entities.clear();// Clear entity selection when data changes

    if (!data_key.isEmpty()) {
        _selected_data.insert(data_key);
        _primary_selected = data_key;
    } else {
        _primary_selected.clear();
    }

    emit selectionChanged(source);
    emit propertiesContextChanged();
}

void SelectionContext::addToSelection(QString const & data_key, SelectionSource const & source) {
    if (data_key.isEmpty()) {
        return;
    }

    bool const was_empty = _selected_data.empty();
    _selected_data.insert(data_key);

    if (was_empty) {
        _primary_selected = data_key;
    }

    emit selectionChanged(source);
}

void SelectionContext::removeFromSelection(QString const & data_key, SelectionSource const & source) {
    auto it = _selected_data.find(data_key);
    if (it == _selected_data.end()) {
        return;
    }

    _selected_data.erase(it);

    // If we removed the primary, pick a new one
    if (_primary_selected == data_key) {
        if (_selected_data.empty()) {
            _primary_selected.clear();
            _selected_entities.clear();
        } else {
            _primary_selected = *_selected_data.begin();
        }
    }

    emit selectionChanged(source);
    emit propertiesContextChanged();
}

void SelectionContext::clearSelection(SelectionSource const & source) {
    if (_selected_data.empty() && _primary_selected.isEmpty()) {
        return;// Nothing to clear
    }

    _selected_data.clear();
    _primary_selected.clear();
    _selected_entities.clear();

    emit selectionChanged(source);
    emit propertiesContextChanged();
}

QString SelectionContext::primarySelectedData() const {
    return _primary_selected;
}

std::set<QString> SelectionContext::allSelectedData() const {
    return _selected_data;
}

bool SelectionContext::isSelected(QString const & data_key) const {
    return _selected_data.contains(data_key);
}

// === Entity Selection ===

void SelectionContext::setSelectedEntities(std::vector<int64_t> const & entity_ids,
                                           SelectionSource const & source) {
    _selected_entities = entity_ids;
    emit entitySelectionChanged(source);
}

void SelectionContext::addSelectedEntities(std::vector<int64_t> const & entity_ids,
                                           SelectionSource const & source) {
    for (auto id : entity_ids) {
        if (std::find(_selected_entities.begin(), _selected_entities.end(), id) ==
            _selected_entities.end()) {
            _selected_entities.push_back(id);
        }
    }
    emit entitySelectionChanged(source);
}

void SelectionContext::clearEntitySelection(SelectionSource const & source) {
    if (_selected_entities.empty()) {
        return;
    }
    _selected_entities.clear();
    emit entitySelectionChanged(source);
}

std::vector<int64_t> SelectionContext::selectedEntities() const {
    return _selected_entities;
}

bool SelectionContext::isEntitySelected(int64_t entity_id) const {
    return std::find(_selected_entities.begin(), _selected_entities.end(), entity_id) !=
           _selected_entities.end();
}

// === Active Editor ===

void SelectionContext::setActiveEditor(QString const & instance_id) {
    if (_active_editor_id != instance_id) {
        _active_editor_id = instance_id;
        emit activeEditorChanged(instance_id);
    }
}

QString SelectionContext::activeEditorId() const {
    return _active_editor_id;
}

// === Properties Context ===

PropertiesContext SelectionContext::propertiesContext() const {
    return PropertiesContext{
            .last_interacted_editor = _last_interacted_editor,
            .selected_data_key = _primary_selected,
            .data_type = _selected_data_type};
}

void SelectionContext::notifyInteraction(QString const & editor_instance_id) {
    if (_last_interacted_editor != editor_instance_id) {
        _last_interacted_editor = editor_instance_id;
        emit propertiesContextChanged();
    }
}

void SelectionContext::setSelectedDataType(QString const & data_type) {
    if (_selected_data_type != data_type) {
        _selected_data_type = data_type;
        emit propertiesContextChanged();
    }
}
