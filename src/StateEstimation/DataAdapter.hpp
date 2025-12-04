#ifndef STATE_ESTIMATION_DATA_ADAPTER_HPP
#define STATE_ESTIMATION_DATA_ADAPTER_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iterator>
#include <ranges>
#include <type_traits>
#include <vector>

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
 * @brief Simple adapter that wraps elements() output for StateEstimation compatibility
 * 
 * This adapter takes a RaggedTimeSeries and provides iteration over FlattenedItem
 * objects, which the StateEstimation tracking system expects.
 * 
 * With the new SoA-based RaggedTimeSeries, elements() already provides a flat
 * view of (TimeFrameIndex, DataEntry<TData>) pairs. This adapter simply converts
 * those to FlattenedItem format.
 * 
 * @tparam DataType The actual data type (e.g., Line2D)
 * 
 * Example usage:
 * @code
 * auto data_source = ElementsDataAdapter<Line2D>(*line_data);
 * 
 * for (auto const& item : data_source) {
 *     TimeFrameIndex time = item.getTimeFrameIndex();
 *     Line2D const& line = item.getData();
 *     EntityId id = item.getEntityId();
 * }
 * @endcode
 */
template<typename DataType>
class ElementsDataAdapter {
public:
    // Store materialized data to ensure stable references
    struct StoredItem {
        TimeFrameIndex time;
        DataType data;  // Owned copy
        EntityId entity_id;
    };
    
    /**
     * @brief Construct from a RaggedTimeSeries-like object with elements() method
     */
    template<typename RaggedTS>
    explicit ElementsDataAdapter(RaggedTS const& ts) {
        // Materialize the elements to ensure stable references
        for (auto const& [time, entry] : ts.elements()) {
            items_.push_back(StoredItem{
                .time = time,
                .data = entry.data,
                .entity_id = entry.entity_id
            });
        }
    }
    
    class Iterator {
    public:
        using value_type = FlattenedItem<DataType>;
        using reference = value_type;
        using pointer = void;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        
        Iterator() = default;
        explicit Iterator(std::vector<StoredItem> const* items, size_t index = 0)
            : items_(items), index_(index) {}
        
        FlattenedItem<DataType> operator*() const {
            auto const& item = (*items_)[index_];
            return FlattenedItem<DataType>{
                .time = item.time,
                .data = item.data,
                .entity_id = item.entity_id
            };
        }
        
        Iterator& operator++() { ++index_; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++index_; return tmp; }
        
        bool operator==(Iterator const& other) const { return index_ == other.index_; }
        bool operator!=(Iterator const& other) const { return index_ != other.index_; }
        
    private:
        std::vector<StoredItem> const* items_ = nullptr;
        size_t index_ = 0;
    };
    
    Iterator begin() const { return Iterator(&items_, 0); }
    Iterator end() const { return Iterator(&items_, items_.size()); }
    
    [[nodiscard]] size_t size() const { return items_.size(); }
    [[nodiscard]] bool empty() const { return items_.empty(); }
    
private:
    std::vector<StoredItem> items_;
};

/**
 * @brief Helper function to create an ElementsDataAdapter from a RaggedTimeSeries
 * 
 * @tparam RaggedTS The RaggedTimeSeries type (e.g., LineData, PointData)
 * @param ts The RaggedTimeSeries to adapt
 * @return An ElementsDataAdapter for the data type
 */
template<typename RaggedTS>
auto makeDataAdapter(RaggedTS const& ts) {
    // Deduce the data type from the elements() return type
    using ElementType = std::ranges::range_value_t<decltype(ts.elements())>;
    using DataEntryType = std::decay_t<decltype(std::declval<ElementType>().second)>;
    using DataType = std::decay_t<decltype(std::declval<DataEntryType>().data)>;
    
    return ElementsDataAdapter<DataType>(ts);
}

// ============================================================================
// Legacy adapter for backwards compatibility (deprecated)
// ============================================================================

/**
 * @brief [DEPRECATED] Adapter that flattens nested time-entry structure into individual items
 * 
 * This adapter was designed for the old getAllEntries() API which returned
 * {TimeFrameIndex, vector<Entry>} pairs. With the new SoA-based elements() API,
 * use ElementsDataAdapter or makeDataAdapter() instead.
 * 
 * @deprecated Use ElementsDataAdapter instead
 */
template<typename Range, typename DataType, typename EntryType>
class FlattenedDataAdapter : public std::ranges::view_interface<FlattenedDataAdapter<Range, DataType, EntryType>> {
public:
    using OuterIterator = std::ranges::iterator_t<Range>;
    using TimeEntriesPair = std::ranges::range_value_t<Range>;
    
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
            auto const& entry = pair.second[inner_idx_];
            return FlattenedItem<DataType>{
                .time = pair.first,
                .data = entry.data,
                .entity_id = entry.entity_id
            };
        }
        
        Iterator& operator++() {
            ++inner_idx_;
            
            auto const& pair = *outer_it_;
            if (inner_idx_ >= pair.second.size()) {
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
                if (!pair.second.empty()) {
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
    mutable Range range_;
};

/**
 * @brief [DEPRECATED] Helper function to create a flattened adapter for LineData ranges
 * 
 * @deprecated Use makeDataAdapter() instead
 */
template<typename Range>
auto flattenLineData(Range&& range) {
    using RangeValueType = std::ranges::range_value_t<std::remove_reference_t<Range>>;
    using EntryType = std::decay_t<decltype(std::declval<RangeValueType>().second[0])>;
    using DataType = std::decay_t<decltype(std::declval<EntryType>().data)>;
    
    return FlattenedDataAdapter<std::remove_reference_t<Range>, DataType, EntryType>(std::forward<Range>(range));
}

} // namespace StateEstimation

// Enable view semantics for FlattenedDataAdapter
template<typename Range, typename DataType, typename EntryType>
inline constexpr bool std::ranges::enable_view<StateEstimation::FlattenedDataAdapter<Range, DataType, EntryType>> = true;

#endif // STATE_ESTIMATION_DATA_ADAPTER_HPP
