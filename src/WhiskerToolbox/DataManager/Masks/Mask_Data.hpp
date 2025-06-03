#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "masks.hpp"

#include <map>
#include <ranges>
#include <vector>

/**
 * @brief The MaskData class
 *
 * MaskData is used for 2D data where the collection of 2D points has *no* order
 * Compare to LineData where the collection of 2D points has an order
 *
 */
class MaskData : public ObserverData {
public:
    MaskData() = default;

    /**
    * @brief Removes all masks at the specified time
    *
    * @param time The timestamp at which to clear masks
    * @param notify If true, observers will be notified of the change
    * @return True if masks existed at the specified time and were cleared, false otherwise
    */
    bool clearAtTime(int time, bool notify = true);

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
    void addAtTime(int time,
                       std::vector<float> const & x,
                       std::vector<float> const & y,
                       bool notify = true);

    /**
    * @brief Adds a new mask at the specified time using a vector of 2D points
    *
    * If masks already exist at the specified time, the new mask is added to the collection.
    * If no masks exist at that time, a new entry is created.
    *
    * @param time The timestamp at which to add the mask
    * @param mask Vector of 2D points defining the mask
    * @param notify If true, observers will be notified of the change (default: true)
    */
    void addAtTime(int time,
                       std::vector<Point2D<float>> mask,
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
    void addAtTime(int time,
                       std::vector<float> && x,
                       std::vector<float> && y,
                       bool notify = true);

    /**
     * @brief Reserve capacity for the internal data structures
     *
     * This can help avoid reallocations when loading large datasets.
     *
     * @param capacity The number of timestamps to reserve space for
     */
    void reserveCapacity(size_t capacity);

    std::vector<int> getTimesWithData() const;

    /**
     * @brief Change the size of the canvas the mask belongs to
     *
     * This will scale all mask in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size
     */
    void changeImageSize(ImageSize const & image_size);

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

    /**
    * @brief Retrieves all masks stored at the specified time
    *
    * @param time The timestamp for which to retrieve masks
    * @return A const reference to a vector of masks at the given time, or an empty vector if no masks exist
    */
    [[nodiscard]] std::vector<Mask2D> const & getAtTime(int time) const;

    /**
     * @brief Get all masks with their associated times as a range
     *
     * @return A view of time-mask pairs for all times
     */
    [[nodiscard]] auto getAllAsRange() const {
        struct TimeMaskPair {
            int time;
            std::vector<Mask2D> const & masks;
        };

        return _data | std::views::transform([](auto const & pair) {
                   return TimeMaskPair{pair.first, pair.second};
               });
    }

protected:
private:
    std::map<int, std::vector<Mask2D>> _data;
    std::vector<Mask2D> _empty{};
    ImageSize _image_size;
};


#endif// MASK_DATA_HPP
