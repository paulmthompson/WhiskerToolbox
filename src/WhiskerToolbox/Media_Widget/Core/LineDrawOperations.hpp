#ifndef LINE_DRAW_OPERATIONS_HPP
#define LINE_DRAW_OPERATIONS_HPP

/**
 * @file LineDrawOperations.hpp
 * @brief Qt-free helpers for committing line geometry from Media Viewer pen tools
 */

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityId.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <optional>
#include <string>
#include <unordered_set>

class LineData;
class Media_Window;
class MediaWidgetState;

/**
 * @brief Convert media-space click coordinates to a LineData key's coordinate system
 * @param x_media X coordinate in media space
 * @param y_media Y coordinate in media space
 * @param scene Media canvas (non-owning)
 * @param state Media widget state for display options (non-owning)
 * @param line_data Target line data object
 * @param line_key Data manager key for display-option lookup
 * @return Point in line data coordinates
 */
[[nodiscard]] Point2D<float> mediaCoordsToLineDataCoords(float x_media,
                                                         float y_media,
                                                         Media_Window const * scene,
                                                         MediaWidgetState const * state,
                                                         LineData const & line_data,
                                                         std::string const & line_key);

/**
 * @brief Add a new line entry at a frame and return the created entity id
 * @param line_data Line data to modify
 * @param time Frame to write at
 * @param line Line geometry to store
 * @param notify Whether to notify observers after the write
 * @return Entity id of the new entry, or nullopt if commit failed
 */
[[nodiscard]] std::optional<EntityId> commitNewLineAtTime(LineData & line_data,
                                                          TimeFrameIndex time,
                                                          Line2D const & line,
                                                          NotifyObservers notify);

/**
 * @brief Whether an entity exists at a specific frame in line data
 * @param line_data Line data to query
 * @param entity_id Entity to test
 * @param time Expected frame
 * @return True when the entity is stored at time
 */
[[nodiscard]] bool entityExistsAtTime(LineData const & line_data,
                                      EntityId entity_id,
                                      TimeFrameIndex time);

#endif// LINE_DRAW_OPERATIONS_HPP
