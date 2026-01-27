#ifndef DATAVIEWER_SELECTION_MANAGER_HPP
#define DATAVIEWER_SELECTION_MANAGER_HPP

/**
 * @file DataViewerSelectionManager.hpp
 * @brief Manages entity selection state for the DataViewer widget
 * 
 * This class extracts selection management from OpenGLWidget to provide
 * a cleaner separation of concerns. It handles:
 * - Multi-select support (Ctrl+click)
 * - Selection state storage
 * - Selection change notifications
 * 
 * The manager emits signals when selection changes, allowing the parent
 * widget and other observers to react appropriately.
 */

#include "Entity/EntityRegistry.hpp"

#include <QObject>

#include <unordered_set>

namespace DataViewer {

/**
 * @brief Manages entity selection state for the DataViewer widget
 * 
 * Provides a clean API for selecting, deselecting, and querying entities.
 * Supports both single-select and multi-select (Ctrl+click) modes.
 */
class DataViewerSelectionManager : public QObject {
    Q_OBJECT

public:
    explicit DataViewerSelectionManager(QObject * parent = nullptr);
    ~DataViewerSelectionManager() override = default;

    /**
     * @brief Select an entity
     * 
     * Adds the entity to the selection set. If already selected, this is a no-op.
     * Emits selectionChanged(id, true) if the entity was not previously selected.
     * 
     * @param id EntityId to select
     */
    void select(EntityId id);

    /**
     * @brief Deselect an entity
     * 
     * Removes the entity from the selection set. If not selected, this is a no-op.
     * Emits selectionChanged(id, false) if the entity was previously selected.
     * 
     * @param id EntityId to deselect
     */
    void deselect(EntityId id);

    /**
     * @brief Toggle selection state of an entity
     * 
     * If the entity is selected, deselects it. If not selected, selects it.
     * Emits selectionChanged with the new state.
     * 
     * @param id EntityId to toggle
     */
    void toggle(EntityId id);

    /**
     * @brief Clear all selections
     * 
     * Removes all entities from the selection set.
     * Emits selectionCleared() followed by selectionChanged for each deselected entity.
     */
    void clear();

    /**
     * @brief Handle entity click with modifier key support
     * 
     * Implements standard selection behavior:
     * - Ctrl+click: Toggle selection of clicked entity (multi-select)
     * - Plain click: Clear selection, then select clicked entity
     * 
     * @param id EntityId that was clicked
     * @param ctrl_pressed true if Ctrl key was held during click
     */
    void handleEntityClick(EntityId id, bool ctrl_pressed);

    /**
     * @brief Check if an entity is currently selected
     * @param id EntityId to check
     * @return true if the entity is in the selection set
     */
    [[nodiscard]] bool isSelected(EntityId id) const;

    /**
     * @brief Get all currently selected entities
     * @return Const reference to the selection set
     */
    [[nodiscard]] std::unordered_set<EntityId> const & selectedEntities() const;

    /**
     * @brief Check if any entities are currently selected
     * @return true if at least one entity is selected
     */
    [[nodiscard]] bool hasSelection() const;

    /**
     * @brief Get the number of selected entities
     * @return Number of entities in the selection set
     */
    [[nodiscard]] size_t selectionCount() const;

signals:
    /**
     * @brief Emitted when an entity's selection state changes
     * @param id The EntityId that changed
     * @param selected true if now selected, false if now deselected
     */
    void selectionChanged(EntityId id, bool selected);

    /**
     * @brief Emitted when all selections are cleared
     * 
     * This is emitted before individual selectionChanged signals for each
     * previously selected entity.
     */
    void selectionCleared();

    /**
     * @brief Emitted when the selection set changes in any way
     * 
     * This is a convenience signal for observers that just need to know
     * that something changed, without caring about specifics.
     */
    void selectionModified();

private:
    std::unordered_set<EntityId> _selected_entities;
};

}// namespace DataViewer

#endif// DATAVIEWER_SELECTION_MANAGER_HPP
