/**
 * @file ContextAction.hpp
 * @brief Context-aware actions for inter-widget communication
 *
 * ContextActions are UI orchestration operations that become available based on the
 * current SelectionContext state. Widgets register actions at startup, and any widget
 * can query applicable actions to show in context menus or toolbars.
 *
 * ContextActions are compositional with Commands: a ContextAction's execute() may
 * invoke Commands internally for the data-mutation step, while handling UI
 * orchestration (opening widgets, configuring views) directly.
 *
 * @see SelectionContext for the registration and query API
 * @see ICommand for serializable/undoable data mutations
 */

#ifndef CONTEXT_ACTION_HPP
#define CONTEXT_ACTION_HPP

#include "StrongTypes.hpp"

#include <QString>

#include <functional>

class SelectionContext;

/**
 * @brief A discoverable action that can be offered based on current context
 *
 * Widgets register ContextActions during startup. When the user right-clicks
 * a data key (or opens an action palette), the system queries all registered
 * actions and shows those whose is_applicable predicate returns true.
 *
 * ## Example
 *
 * ```cpp
 * ContextAction action;
 * action.action_id = "scatter_plot.visualize_2d";
 * action.display_name = "Visualize in Scatter Plot";
 * action.is_applicable = [dm](SelectionContext const & ctx) {
 *     auto * tensor = dm->getData<TensorData>(ctx.dataFocus().toStdString());
 *     return tensor && tensor->numColumns() >= 2;
 * };
 * action.execute = [registry](SelectionContext const & ctx) {
 *     // Open scatter plot, configure axes from focused tensor
 * };
 * selection_context->registerAction(std::move(action));
 * ```
 */
struct ContextAction {
    QString action_id;                                          ///< Unique identifier (e.g., "scatter_plot.visualize_2d")
    QString display_name;                                       ///< User-visible label (e.g., "Visualize in Scatter Plot")
    QString icon;                                               ///< Optional icon resource path
    EditorLib::EditorTypeId producer_type;                      ///< Widget type that provides this action
    std::function<bool(SelectionContext const &)> is_applicable;///< Predicate: can this action run in the current context?
    std::function<void(SelectionContext const &)> execute;      ///< Execute the action
};

#endif// CONTEXT_ACTION_HPP
