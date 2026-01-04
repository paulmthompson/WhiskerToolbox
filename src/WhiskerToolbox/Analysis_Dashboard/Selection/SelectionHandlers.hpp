#ifndef SELECTIONHANDLERS_HPP
#define SELECTIONHANDLERS_HPP

#include <memory>

class ISelectionHandler;

/**
 * @brief Type alias for selection handler pointer
 * 
 * This replaces the previous SelectionVariant (std::variant) with a simple
 * unique_ptr to the ISelectionHandler interface. This simplifies widget code
 * by eliminating the need for std::visit.
 * 
 * The old variant type was:
 *   using SelectionVariant = std::variant<
 *       std::unique_ptr<LineSelectionHandler>,
 *       std::unique_ptr<NoneSelectionHandler>,
 *       std::unique_ptr<PolygonSelectionHandler>,
 *       std::unique_ptr<PointSelectionHandler>>;
 * 
 * @see ISelectionHandler for the unified interface
 */
using SelectionHandler = std::unique_ptr<ISelectionHandler>;

// Keep the old name for backward compatibility during migration
// TODO: Remove this alias once all usages are updated
using SelectionVariant = SelectionHandler;

#endif// SELECTIONHANDLERS_HPP