#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TypeTraits/DataTypeTraits.hpp"
#include "utils/RaggedTimeSeries.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <vector>


/**
 * @brief Structure holding a Line2D and its associated EntityId
 */
using LineEntry = DataEntry<Line2D>;

/*
 * @brief LineData
 *
 * LineData is used for storing 2D lines
 * Line data implies that the elements in the line have an order
 * Compare to MaskData where the elements in the mask have no order
 */
class LineData : public RaggedTimeSeries<Line2D> {
public:
    // ========== Type Traits ==========
    /**
     * @brief Type traits for LineData
     * 
     * Defines compile-time properties of LineData for use in generic algorithms
     * and the transformation system.
     */
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<LineData, Line2D> {
        static constexpr bool is_ragged = true;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = true;
        static constexpr bool is_spatial = true;
    };

    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty LineData with no data
     */
    LineData() = default;

    /**
     * @brief Constructor with data
     * 
     * This constructor creates a LineData with the given data
     * 
     * @param data The data to initialize the LineData with
     */
    explicit LineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data);

    // ========== Image Size ==========

    /**
     * @brief Change the size of the canvas the line belongs to
     *
     * This will scale all lines in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size
     */
    void changeImageSize(ImageSize const & image_size);
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }
};

using LineDataView = RaggedTimeSeriesView<Line2D>;

#endif// LINE_DATA_HPP
