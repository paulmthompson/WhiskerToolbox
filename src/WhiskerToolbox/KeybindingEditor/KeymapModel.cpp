/**
 * @file KeymapModel.cpp
 * @brief Implementation of the two-level tree model for keybinding display
 */

#include "KeymapModel.hpp"

#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeymapManager.hpp"

#include <QColor>
#include <QFont>

#include <algorithm>
#include <map>
#include <set>

KeymapModel::KeymapModel(KeymapSystem::KeymapManager * manager, QObject * parent)
    : QAbstractItemModel(parent),
      _manager(manager) {
    refresh();
}

// --- Tree structure ---

QModelIndex KeymapModel::index(int row, int column, QModelIndex const & parent) const {
    if (!hasIndex(row, column, parent)) {
        return {};
    }

    if (!parent.isValid()) {
        // Top-level: category row. Internal pointer is null (category index = row).
        return createIndex(row, column, quintptr(0));
    }

    // Child of a category: action row. Encode parent category index + 1 in internal id.
    auto const category_idx = static_cast<quintptr>(parent.row() + 1);
    return createIndex(row, column, category_idx);
}

QModelIndex KeymapModel::parent(QModelIndex const & child) const {
    if (!child.isValid()) {
        return {};
    }

    auto const id = child.internalId();
    if (id == 0) {
        // Category row — no parent
        return {};
    }

    // Action row: internal id = category index + 1
    auto const category_row = static_cast<int>(id - 1);
    return createIndex(category_row, 0, quintptr(0));
}

int KeymapModel::rowCount(QModelIndex const & parent) const {
    if (!parent.isValid()) {
        return static_cast<int>(_categories.size());
    }

    // Only categories (top-level) have children
    if (parent.internalId() != 0) {
        return 0;// Action rows have no children
    }

    auto const cat_idx = static_cast<size_t>(parent.row());
    if (cat_idx >= _categories.size()) {
        return 0;
    }
    return static_cast<int>(_categories[cat_idx].actions.size());
}

int KeymapModel::columnCount(QModelIndex const & /*parent*/) const {
    return ColumnCount;
}

// --- Data ---

QVariant KeymapModel::data(QModelIndex const & index, int role) const {
    if (!index.isValid()) {
        return {};
    }

    bool const is_category = (index.internalId() == 0);

    if (is_category) {
        auto const cat_idx = static_cast<size_t>(index.row());
        if (cat_idx >= _categories.size()) {
            return {};
        }
        auto const & cat = _categories[cat_idx];

        switch (role) {
            case Qt::DisplayRole:
                if (index.column() == ColName) {
                    return cat.name;
                }
                return {};
            case Qt::FontRole:
                if (index.column() == ColName) {
                    QFont f;
                    f.setBold(true);
                    return f;
                }
                return {};
            case IsCategoryRole:
                return true;
            case ActionIdRole:
                return QString{};
            default:
                return {};
        }
    }

    // Action row
    auto const category_idx = static_cast<size_t>(index.internalId() - 1);
    if (category_idx >= _categories.size()) {
        return {};
    }
    auto const & cat = _categories[category_idx];
    auto const action_idx = static_cast<size_t>(index.row());
    if (action_idx >= cat.actions.size()) {
        return {};
    }
    auto const & action = cat.actions[action_idx];

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case ColName:
                    return action.display_name;
                case ColBinding:
                    if (action.effective_binding.isEmpty()) {
                        return QStringLiteral("(unbound)");
                    }
                    return action.effective_binding.toString(QKeySequence::NativeText);
                case ColScope:
                    return action.scope_label;
                case ColDefault:
                    if (action.default_binding.isEmpty()) {
                        return QStringLiteral("(none)");
                    }
                    return action.default_binding.toString(QKeySequence::NativeText);
                default:
                    return {};
            }
        case Qt::ForegroundRole:
            if (action.has_conflict) {
                return QColor(Qt::red);
            }
            if (index.column() == ColBinding && action.effective_binding.isEmpty()) {
                return QColor(Qt::gray);
            }
            if (index.column() == ColDefault && action.default_binding.isEmpty()) {
                return QColor(Qt::gray);
            }
            if (action.is_override && index.column() == ColBinding) {
                return QColor(0, 0, 180);// Blue for overridden bindings
            }
            return {};
        case Qt::FontRole:
            if (action.is_override && index.column() == ColBinding) {
                QFont f;
                f.setItalic(true);
                return f;
            }
            return {};
        case Qt::ToolTipRole:
            if (action.has_conflict) {
                return QStringLiteral("Warning: binding conflict detected");
            }
            if (action.is_override) {
                return QStringLiteral("User override active (default: %1)")
                        .arg(action.default_binding.isEmpty()
                                     ? QStringLiteral("none")
                                     : action.default_binding.toString(QKeySequence::NativeText));
            }
            return {};
        case ActionIdRole:
            return action.action_id;
        case IsCategoryRole:
            return false;
        case HasConflictRole:
            return action.has_conflict;
        case IsOverrideRole:
            return action.is_override;
        default:
            return {};
    }
}

QVariant KeymapModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
        case ColName:
            return QStringLiteral("Action");
        case ColBinding:
            return QStringLiteral("Binding");
        case ColScope:
            return QStringLiteral("Scope");
        case ColDefault:
            return QStringLiteral("Default");
        default:
            return {};
    }
}

Qt::ItemFlags KeymapModel::flags(QModelIndex const & index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// --- Refresh ---

void KeymapModel::refresh() {
    beginResetModel();
    _categories.clear();

    if (!_manager) {
        endResetModel();
        return;
    }

    auto const actions = _manager->allActions();

    // Build conflict set for quick lookup
    auto const conflicts = _manager->detectConflicts();
    std::set<QString> conflicting_ids;
    for (auto const & c: conflicts) {
        conflicting_ids.insert(c.action_id_a);
        conflicting_ids.insert(c.action_id_b);
    }

    // Group actions by category (preserving insertion order via ordered map)
    std::map<QString, std::vector<ActionItem>> grouped;
    for (auto const & desc: actions) {
        auto const binding = _manager->bindingFor(desc.action_id);
        bool const has_override =
                (binding != desc.default_binding) ||
                (!_manager->isActionEnabled(desc.action_id) && !desc.default_binding.isEmpty());

        grouped[desc.category].push_back({
                .action_id = desc.action_id,
                .display_name = desc.display_name,
                .scope_label = _scopeLabel(desc),
                .effective_binding = binding,
                .default_binding = desc.default_binding,
                .has_conflict = conflicting_ids.contains(desc.action_id),
                .is_override = has_override,
        });
    }

    // Sort actions within each category by display name
    for (auto & [cat_name, items]: grouped) {
        std::sort(items.begin(), items.end(), [](ActionItem const & a, ActionItem const & b) {
            return a.display_name.compare(b.display_name, Qt::CaseInsensitive) < 0;
        });
        _categories.push_back({.name = cat_name, .actions = std::move(items)});
    }

    endResetModel();
}

// --- Helpers ---

QString KeymapModel::actionIdAt(QModelIndex const & index) const {
    return data(index, ActionIdRole).toString();
}

QString KeymapModel::_scopeLabel(KeymapSystem::KeyActionDescriptor const & desc) {
    switch (desc.scope.kind) {
        case KeymapSystem::KeyActionScopeKind::Global:
            return QStringLiteral("Global");
        case KeymapSystem::KeyActionScopeKind::EditorFocused:
            return QStringLiteral("Focused (%1)").arg(desc.scope.editor_type_id.value);
        case KeymapSystem::KeyActionScopeKind::AlwaysRouted:
            return QStringLiteral("Always (%1)").arg(desc.scope.editor_type_id.value);
    }
    return QStringLiteral("Unknown");
}
