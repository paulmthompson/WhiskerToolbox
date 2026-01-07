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
#include "TypeTraits/DataTypeTraits.hpp"
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
    // ========== Type Traits ==========
    /**
     * @brief Type traits for MaskData
     * 
     * Defines compile-time properties of MaskData for use in generic algorithms
     * and the transformation system.
     */
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<MaskData, Mask2D> {
        static constexpr bool is_ragged = true;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = true;
        static constexpr bool is_spatial = true;
    };

    // ========== Constructors ==========
    MaskData() = default;
    
    // Inherit range constructor from base class for view-based construction
    using RaggedTimeSeries<Mask2D>::RaggedTimeSeries;

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

    // ========== Copy / Move with scaling ==========
    /**
     * Copy selected masks into another MaskData, resampling using nearest-neighbor
     * dst->src mapping when source and target image sizes differ.
     */
    std::size_t copyByEntityIds(MaskData & target,
                                std::unordered_set<EntityId> const & entity_ids,
                                NotifyObservers notify);

    /**
     * Move selected masks into another MaskData, resampling using nearest-neighbor
     * dst->src mapping when source and target image sizes differ. EntityIds are preserved.
     */
    std::size_t moveByEntityIds(MaskData & target,
                                std::unordered_set<EntityId> const & entity_ids,
                                NotifyObservers notify);
};

using MaskDataView = RaggedTimeSeriesView<Mask2D>;

#endif// MASK_DATA_HPP
