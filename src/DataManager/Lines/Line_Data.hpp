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

/*
 * @brief LineData
 *
 * LineData is used for storing 2D lines
 * Line data implies that the elements in the line have an order
 * Compare to MaskData where the elements in the mask have no order
 */
class LineData : public RaggedTimeSeries<Line2D> {
public:
    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty LineData with no data
     */
    LineData() = default;

    /**
     * @brief Move constructor
     */
    LineData(LineData && other) noexcept;

    /**
     * @brief Move assignment operator
     */
    LineData & operator=(LineData && other) noexcept;


    /**
     * @brief Constructor with data
     * 
     * This constructor creates a LineData with the given data
     * 
     * @param data The data to initialize the LineData with
     */
    explicit LineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data);

    // ========== Setters (Time-based) ==========

    /**
     * @brief Add a line at a specific time (by copying).
     *
     * This overload is called when you pass an existing lvalue (e.g., a named variable).
     * It will create a copy of the line.
     * 
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeFrameIndex time, Line2D const & line, NotifyObservers notify);

    void addAtTime(TimeIndexAndFrame const & time_index_and_frame, Line2D const & line, NotifyObservers notify);

    /**
     * @brief Add a line at a specific time (by moving).
     *
     * This overload is called when you pass an rvalue (e.g., a temporary object
     * or the result of std::move()). It will move the line's data,
     * avoiding a copy.
     * 
     * @param notify Whether to notify observers after the operation
     */

    void addAtTime(TimeFrameIndex time, Line2D && line, NotifyObservers notify);
    
    void addAtTime(TimeIndexAndFrame const & time_index_and_frame, Line2D && line, NotifyObservers notify);

    /**
     * @brief Construct a data entry in-place at a specific time.
     *
     * This method perfectly forwards its arguments to the
     * constructor of the TData (e.g., Line2D) object.
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
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
        }

        _data[time].emplace_back(entity_id, std::forward<TDataArgs>(args)...);
    }

    /**
     * @brief Add a batch of lines at a specific time by copying them.
     *
     * Appends the lines to any already existing at that time.
     */
    void addAtTime(TimeFrameIndex time, std::vector<Line2D> const & lines_to_add);

    /**
     * @brief Add a batch of lines at a specific time by moving them.
     *
     * Appends the lines to any already existing at that time.
     * The input vector will be left in a state with "empty" lines.
     */
    void addAtTime(TimeFrameIndex time, std::vector<Line2D> && lines_to_add);

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


#endif// LINE_DATA_HPP
