#ifndef MEDIA_SELECTION_HIT_HPP
#define MEDIA_SELECTION_HIT_HPP

/**
 * @file MediaSelectionHit.hpp
 * @brief Result of a unified cross-type entity hit-test in the Media Viewer
 */

#include "Entity/EntityTypes.hpp"

#include <string>

/**
 * @brief Closest selectable entity at a canvas position
 */
struct MediaSelectionHit {
    EntityId entity_id{};
    std::string data_key;
    std::string data_type;///< "line", "point", or "mask"
    double distance_px{}; ///< Scene-space distance used for ranking
};

#endif// MEDIA_SELECTION_HIT_HPP
