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
};


#endif// MASK_DATA_HPP
