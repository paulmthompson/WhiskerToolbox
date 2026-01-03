#include "DataViewerSelectionManager.hpp"

namespace DataViewer {

DataViewerSelectionManager::DataViewerSelectionManager(QObject * parent)
    : QObject(parent) {}

void DataViewerSelectionManager::select(EntityId id) {
    auto [_, inserted] = _selected_entities.insert(id);
    if (inserted) {
        emit selectionChanged(id, true);
        emit selectionModified();
    }
}

void DataViewerSelectionManager::deselect(EntityId id) {
    auto const removed = _selected_entities.erase(id);
    if (removed > 0) {
        emit selectionChanged(id, false);
        emit selectionModified();
    }
}

void DataViewerSelectionManager::toggle(EntityId id) {
    if (_selected_entities.contains(id)) {
        deselect(id);
    } else {
        select(id);
    }
}

void DataViewerSelectionManager::clear() {
    if (_selected_entities.empty()) {
        return;
    }

    // Collect IDs before clearing (since we need to emit signals for each)
    std::vector<EntityId> previously_selected(_selected_entities.begin(), _selected_entities.end());

    _selected_entities.clear();
    emit selectionCleared();

    // Emit individual change signals for each previously selected entity
    for (EntityId const id: previously_selected) {
        emit selectionChanged(id, false);
    }

    emit selectionModified();
}

void DataViewerSelectionManager::handleEntityClick(EntityId id, bool ctrl_pressed) {
    if (ctrl_pressed) {
        // Multi-select mode: toggle the clicked entity
        toggle(id);
    } else {
        // Single-select mode: clear and select
        // Emit changes for previously selected entities
        std::vector<EntityId> previously_selected(_selected_entities.begin(), _selected_entities.end());

        _selected_entities.clear();
        emit selectionCleared();

        for (EntityId const old_id: previously_selected) {
            if (old_id != id) {
                emit selectionChanged(old_id, false);
            }
        }

        // Select the new entity
        _selected_entities.insert(id);
        emit selectionChanged(id, true);
        emit selectionModified();
    }
}

bool DataViewerSelectionManager::isSelected(EntityId id) const {
    return _selected_entities.contains(id);
}

std::unordered_set<EntityId> const & DataViewerSelectionManager::selectedEntities() const {
    return _selected_entities;
}

bool DataViewerSelectionManager::hasSelection() const {
    return !_selected_entities.empty();
}

size_t DataViewerSelectionManager::selectionCount() const {
    return _selected_entities.size();
}

}// namespace DataViewer
