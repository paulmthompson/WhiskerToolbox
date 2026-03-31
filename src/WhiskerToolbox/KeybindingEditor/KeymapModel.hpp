/**
 * @file KeymapModel.hpp
 * @brief QAbstractItemModel providing a two-level tree of key actions grouped by category
 *
 * The model reads from KeymapManager and exposes a tree structure:
 *   Category (e.g. "Timeline", "Media Viewer")
 *     └── Action (e.g. "Play / Pause", "Assign to Group 1")
 *
 * Columns: Action Name | Binding | Scope | Default
 *
 * @see KeymapManager for the data source
 * @see KeybindingEditor for the consumer
 */

#ifndef KEYMAP_MODEL_HPP
#define KEYMAP_MODEL_HPP

#include <QAbstractItemModel>
#include <QKeySequence>
#include <QString>

#include <vector>

namespace KeymapSystem {
class KeymapManager;
struct KeyActionDescriptor;
}// namespace KeymapSystem

/**
 * @brief Two-level tree model mapping KeymapManager actions into Category → Action rows
 *
 * Thread safety: not thread-safe. Must be used from the GUI thread only.
 */
class KeymapModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,///< Display name / category name
        ColBinding, ///< Current effective binding
        ColScope,   ///< Scope kind label (Global, Focused, Always-Routed)
        ColDefault, ///< Default binding (for reference)
        ColumnCount
    };

    /// Custom roles for programmatic access
    enum Roles {
        ActionIdRole = Qt::UserRole + 1,///< QString action_id (actions only)
        IsCategoryRole,                 ///< bool — true for category rows
        HasConflictRole,                ///< bool — true if this action has a binding conflict
        IsOverrideRole                  ///< bool — true if user override is active
    };

    explicit KeymapModel(KeymapSystem::KeymapManager * manager, QObject * parent = nullptr);

    // --- QAbstractItemModel interface ---
    [[nodiscard]] QModelIndex index(int row, int column, QModelIndex const & parent = {}) const override;
    [[nodiscard]] QModelIndex parent(QModelIndex const & child) const override;
    [[nodiscard]] int rowCount(QModelIndex const & parent = {}) const override;
    [[nodiscard]] int columnCount(QModelIndex const & parent = {}) const override;
    [[nodiscard]] QVariant data(QModelIndex const & index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(QModelIndex const & index) const override;

    /// Rebuild the model data from KeymapManager
    void refresh();

    /// Get action_id for a model index (empty for category rows)
    [[nodiscard]] QString actionIdAt(QModelIndex const & index) const;

private:
    /// A single action entry within a category
    struct ActionItem {
        QString action_id;
        QString display_name;
        QString scope_label;
        QKeySequence effective_binding;
        QKeySequence default_binding;
        bool has_conflict = false;
        bool is_override = false;
    };

    /// A category node containing zero or more actions
    struct CategoryItem {
        QString name;
        std::vector<ActionItem> actions;
    };

    KeymapSystem::KeymapManager * _manager = nullptr;
    std::vector<CategoryItem> _categories;

    /// Build the scope label string from a descriptor
    [[nodiscard]] static QString _scopeLabel(KeymapSystem::KeyActionDescriptor const & desc);
};

#endif// KEYMAP_MODEL_HPP
