#ifndef SELECTIONHANDLERS_HPP
#define SELECTIONHANDLERS_HPP

#include <memory>
#include <variant>

class NoneSelectionHandler;
class PolygonSelectionHandler;
class LineSelectionHandler;
class PointSelectionHandler;

using SelectionVariant = std::variant<std::unique_ptr<LineSelectionHandler>,
                                      std::unique_ptr<NoneSelectionHandler>,
                                      std::unique_ptr<PolygonSelectionHandler>,
                                      std::unique_ptr<PointSelectionHandler>>;


#endif// SELECTIONHANDLERS_HPP