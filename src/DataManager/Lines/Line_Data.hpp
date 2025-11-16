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

/**
 * @brief LineData - Base class for line storage
 *
 * LineData is the base class for storing 2D lines in different storage patterns.
 * Line data implies that the elements in the line have an order.
 * Compare to MaskData where the elements in the mask have no order.
 * 
 * This class follows the MediaData pattern where the base class provides
 * the interface and common functionality, while derived classes provide
 * specific storage implementations:
 * - RaggedLineData: Variable-sized vectors at each time point (current default)
 * - FixedSizeLineData: Fixed-size arrays at each time point (future)
 * - SingleLineData: Exactly one line per time point (future)
 * 
 * By default, LineData behaves as RaggedLineData (for backward compatibility).
 * Derived classes can override getLineDataType() to indicate their specific type.
 */
class LineData : public RaggedTimeSeries<Line2D> {
public:
    /**
     * @brief Enum for line data storage types
     */
    enum class LineDataType {
        Ragged,      ///< Variable number of lines per time point (default)
        FixedSize,   ///< Fixed number of lines per time point
        Single       ///< Exactly one line per time point
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

    /**
     * @brief Virtual destructor
     */
    virtual ~LineData() = default;

    /**
     * @brief Get the line data storage type
     * @return LineDataType enum value indicating the storage pattern
     */
    virtual LineDataType getLineDataType() const { return LineDataType::Ragged; }

    // ========== Image Size ==========

    /**
     * @brief Change the size of the canvas the line belongs to
     *
     * This will scale all lines in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size The new image size
     */
    void changeImageSize(ImageSize const & image_size);
    
    /**
     * @brief Set the image size
     * 
     * @param image_size The image size to set
     */
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }
};


#endif// LINE_DATA_HPP
