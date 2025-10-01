#ifndef STATE_ESTIMATION_DATA_ADAPTER_HPP
#define STATE_ESTIMATION_DATA_ADAPTER_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iterator>
#include <ranges>
#include <type_traits>

namespace StateEstimation {

/**
 * @brief A single flattened data item with associated metadata for tracking
 * 
 * This structure provides zero-copy access to data while copying
 * cheap metadata (TimeFrameIndex, EntityId).
 * 
 * @tparam DataType The actual data type (e.g., Line2D)
 */
template<typename DataType>
struct FlattenedItem {
    TimeFrameIndex time;
    DataType const& data;  // Reference for zero-copy access
    EntityId entity_id;
    
    // Accessor methods for DataSource concept compatibility
    DataType const& getData() const { return data; }
    EntityId getEntityId() const { return entity_id; }
    TimeFrameIndex getTimeFrameIndex() const { return time; }
};

/**
 * @brief Adapter that flattens nested time-entry structure into individual items
 * 
 * This adapter converts a range of {TimeFrameIndex, vector<Entry>} pairs into
 * a flat iteration of individual DataItem objects. Each entry in the nested
 * structure is yielded as a separate item with associated time and entity metadata.
 * 
 * The adapter maintains zero-copy semantics for the actual data objects by
 * holding references, while copying cheap metadata (TimeFrameIndex, EntityId).
 * 
 * @tparam Range The type of the input range (e.g., from GetAllLineEntriesAsRange)
 * @tparam DataType The actual data type contained in entries (e.g., Line2D)
 * @tparam EntryType The entry structure type (must have .line and .entity_id members)
 * 
 * Example usage:
 * @code
 * auto line_range = line_data->GetAllLineEntriesAsRange();
 * auto flat_range = FlattenedDataAdapter(line_range);
 * 
 * for (auto const& item : flat_range) {
 *     TimeFrameIndex time = item.getTimeFrameIndex();
 *     Line2D const& line = item.getData();  // Zero-copy reference
 *     EntityId id = item.getEntityId();
 * }
 * @endcode
 */
template<typename Range, typename DataType, typename EntryType>
class FlattenedDataAdapter : public std::ranges::view_interface<FlattenedDataAdapter<Range, DataType, EntryType>> {
public:
    using OuterIterator = std::ranges::iterator_t<Range>;
    using TimeEntriesPair = std::ranges::range_value_t<Range>;
    
    /**
     * @brief Iterator for flattened data
     * 
     * Maintains position in both the outer range (time frames) and 
     * inner vector (entries within each time frame).
     */
    class Iterator {
    public:
        using value_type = FlattenedItem<DataType>;
        using reference = value_type;
        using pointer = value_type const*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        
        Iterator() = default;
        
        Iterator(OuterIterator outer_begin, OuterIterator outer_end)
            : outer_it_(outer_begin), outer_end_(outer_end), inner_idx_(0) {
            skipEmptyFrames();
        }
        
        FlattenedItem<DataType> operator*() const {
            auto const& pair = *outer_it_;
            auto const& entry = pair.entries[inner_idx_];
            return FlattenedItem<DataType>{
                .time = pair.time,
                .data = entry.line,
                .entity_id = entry.entity_id
            };
        }
        
        Iterator& operator++() {
            ++inner_idx_;
            
            // Check if we've exhausted the current frame's entries
            auto const& pair = *outer_it_;
            if (inner_idx_ >= pair.entries.size()) {
                ++outer_it_;
                inner_idx_ = 0;
                skipEmptyFrames();
            }
            
            return *this;
        }
        
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(Iterator const& other) const {
            if (outer_it_ != other.outer_it_) {
                return false;
            }
            // If both are at end, they're equal regardless of inner_idx
            if (outer_it_ == outer_end_) {
                return true;
            }
            return inner_idx_ == other.inner_idx_;
        }
        
        bool operator!=(Iterator const& other) const {
            return !(*this == other);
        }
        
    private:
        void skipEmptyFrames() {
            while (outer_it_ != outer_end_) {
                auto const& pair = *outer_it_;
                if (!pair.entries.empty()) {
                    break;
                }
                ++outer_it_;
            }
        }
        
        OuterIterator outer_it_;
        OuterIterator outer_end_;
        size_t inner_idx_;
    };
    
    explicit FlattenedDataAdapter(Range range)
        : range_(std::move(range)) {}
    
    Iterator begin() { 
        return Iterator(std::ranges::begin(range_), std::ranges::end(range_)); 
    }
    
    Iterator end() { 
        return Iterator(std::ranges::end(range_), std::ranges::end(range_)); 
    }
    
    Iterator begin() const { 
        return Iterator(std::ranges::begin(range_), std::ranges::end(range_)); 
    }
    
    Iterator end() const { 
        return Iterator(std::ranges::end(range_), std::ranges::end(range_)); 
    }
    
private:
    mutable Range range_;  // Store by value to handle temporary ranges/views, mutable for const iteration
};

/**
 * @brief Helper function to create a flattened adapter for LineData ranges
 * 
 * This factory function deduces template parameters automatically and creates
 * a FlattenedDataAdapter suitable for use with LineData's GetAllLineEntriesAsRange().
 * 
 * @tparam Range The range type (auto-deduced)
 * @param range The input range from GetAllLineEntriesAsRange()
 * @return A FlattenedDataAdapter that yields individual DataItem<Line2D> objects
 * 
 * Example:
 * @code
 * auto line_range = line_data->GetAllLineEntriesAsRange();
 * auto data_source = flattenLineData(line_range);
 * 
 * Tracker<Line2D> tracker(...);
 * tracker.process(data_source, ...);
 * @endcode
 */
template<typename Range>
auto flattenLineData(Range&& range) {
    // Deduce types from the range
    using RangeValueType = std::ranges::range_value_t<std::remove_reference_t<Range>>;
    // Assume entries have .line member which is Line2D
    using EntryType = std::decay_t<decltype(std::declval<RangeValueType>().entries[0])>;
    using DataType = std::decay_t<decltype(std::declval<EntryType>().line)>;
    
    return FlattenedDataAdapter<std::remove_reference_t<Range>, DataType, EntryType>(std::forward<Range>(range));
}

} // namespace StateEstimation

// Enable view semantics for FlattenedDataAdapter
template<typename Range, typename DataType, typename EntryType>
inline constexpr bool std::ranges::enable_view<StateEstimation::FlattenedDataAdapter<Range, DataType, EntryType>> = true;

#endif // STATE_ESTIMATION_DATA_ADAPTER_HPP
