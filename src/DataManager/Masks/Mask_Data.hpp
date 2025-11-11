#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "utils/RaggedTimeSeries.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_set>
#include <vector>

using MaskEntry = DataEntry<Mask2D>;


/**
 * @brief The MaskData class
 *
 * MaskData is used for 2D data where the collection of 2D points has *no* order
 * Compare to LineData where the collection of 2D points has an order
 *
 */
class MaskData : public RaggedTimeSeries<Mask2D> {
public:
    // ========== Constructors ==========
    MaskData() = default;

    // ========== Setters (Time-based) ==========

    [[nodiscard]] bool clearAtTime(TimeIndexAndFrame const & time_index_and_frame,
                                   NotifyObservers notify);


    void addAtTime(TimeFrameIndex time, Mask2D const & mask, bool notify = true);

    void addAtTime(TimeFrameIndex time, Mask2D && mask, bool notify = true);

    /**
     * @brief Construct a data entry in-place at a specific time.
     *
     * This method perfectly forwards its arguments to the
     * constructor of the TData (e.g., Mask2D) object.
     * This is the most efficient way to add new data.
     *
     * @tparam TDataArgs The types of arguments for TData's constructor
     * @param time The time to add the data at
     * @param args The arguments to forward to TData's constructor
     */
    template<typename... TDataArgs>
    void emplaceAtTime(TimeFrameIndex time, TDataArgs &&... args) {
        int const local_index = static_cast<int>(_data[time].size());
        EntityId entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
        }

        _data[time].emplace_back(entity_id, std::forward<TDataArgs>(args)...);
    }

    /**
     * @brief Adds a new mask at the specified time using separate x and y coordinate arrays
    *
    * If masks already exist at the specified time, the new mask is added to the collection.
    * If no masks exist at that time, a new entry is created.
    *
    * @param time The timestamp at which to add the mask
    * @param x Vector of x-coordinates defining the mask points
    * @param y Vector of y-coordinates defining the mask points
    * @param notify If true, observers will be notified of the change (default: true)
    *
    * @note x and y vectors must be the same length, representing coordinate pairs
    */
    void addAtTime(TimeFrameIndex time,
                   std::vector<uint32_t> const & x,
                   std::vector<uint32_t> const & y,
                   bool notify = true);


    void addAtTime(TimeIndexAndFrame const & time_index_and_frame,
                   std::vector<Point2D<uint32_t>> mask,
                   bool notify = true);

    /**
     * @brief Adds a new mask at the specified time using separate x and y coordinate arrays (move version)
    *
    * This overload avoids copying the input vectors by moving them.
    * If masks already exist at the specified time, the new mask is added to the collection.
    * If no masks exist at that time, a new entry is created.
    *
    * @param time The timestamp at which to add the mask
    * @param x Vector of x-coordinates defining the mask points (will be moved)
    * @param y Vector of y-coordinates defining the mask points (will be moved)
    * @param notify If true, observers will be notified of the change (default: true)
    *
    * @note x and y vectors must be the same length, representing coordinate pairs
    */
    void addAtTime(TimeFrameIndex time,
                   std::vector<uint32_t> && x,
                   std::vector<uint32_t> && y,
                   bool notify = true);

    // ========== Setters (Entity-based) ==========

    /**
     * @brief Removes a mask with a specific EntityId
     *
     * @param entity_id The EntityId of the mask to remove
     * @param notify If true, observers will be notified of the change
     * @return True if the mask was found and removed, false otherwise
     */
    [[nodiscard]] bool clearByEntityId(EntityId entity_id, NotifyObservers notify);


    // ========== Getters (Time-based) ==========

    [[nodiscard]] size_t size() const { return _data.size(); };


    // ========== Image Size ==========
    /**
     * @brief Change the size of the canvas the mask belongs to
     *
     * This will scale all mask in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size
     */
    void changeImageSize(ImageSize const & image_size);

    /**
     * @brief Set the image size
     */
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

private:
    mutable std::vector<Mask2D> _temp_masks{};

    /**
    * @brief Removes all masks at the specified time
    *
    * @param time The timestamp at which to clear masks
    * @param notify If true, observers will be notified of the change
    * @return True if masks existed at the specified time and were cleared, false otherwise
    */
    [[nodiscard]] bool _clearAtTime(TimeFrameIndex time, NotifyObservers notify);
};


#endif// MASK_DATA_HPP
