#ifndef STATE_ESTIMATION_MASK_DATA_ADAPTER_HPP
#define STATE_ESTIMATION_MASK_DATA_ADAPTER_HPP

#include "CoreGeometry/masks.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iterator>
#include <ranges>
#include <vector>

namespace StateEstimation {

/**
 * @brief A single flattened mask data item with associated metadata for tracking
 * 
 * This structure provides zero-copy access to mask data while copying
 * cheap metadata (TimeFrameIndex).
 * 
 * @note Unlike LineData which has EntityId per entry, masks may have multiple
 * masks per time with different entity IDs. This adapter provides the full
 * vector of masks at each time point.
 */
struct FlattenedMaskItem {
    TimeFrameIndex time;
    std::vector<Mask2D> const& masks;  // Reference for zero-copy access
    
    // Accessor methods for DataSource concept compatibility
    std::vector<Mask2D> const& getData() const { return masks; }
    TimeFrameIndex getTimeFrameIndex() const { return time; }
};

/**
 * @brief Adapter that flattens time-mask structure into individual time-mask pairs
 * 
 * This adapter converts a range of {TimeFrameIndex, vector<Mask2D>} pairs into
 * a flat iteration of FlattenedMaskItem objects. Each time point with masks is
 * yielded as a separate item.
 * 
 * The adapter maintains zero-copy semantics for the actual mask data by
 * holding references, while copying cheap metadata (TimeFrameIndex).
 * 
 * @tparam Range The type of the input range (e.g., from MaskData::getAllAsRange)
 * 
 * Example usage:
 * @code
 * auto mask_range = mask_data->getAllAsRange();
 * auto flat_range = FlattenedMaskAdapter(mask_range);
 * 
 * for (auto const& item : flat_range) {
 *     TimeFrameIndex time = item.getTimeFrameIndex();
 *     std::vector<Mask2D> const& masks = item.getData();  // Zero-copy reference
 * }
 * @endcode
 */
template<typename Range>
class FlattenedMaskAdapter : public std::ranges::view_interface<FlattenedMaskAdapter<Range>> {
public:
    using OuterIterator = std::ranges::iterator_t<Range>;
    using TimeMasksPair = std::ranges::range_value_t<Range>;
    
    /**
     * @brief Iterator for flattened mask data
     * 
     * Iterates over each time point that has mask data.
     */
    class Iterator {
    public:
        using value_type = FlattenedMaskItem;
        using reference = value_type;
        using pointer = value_type const*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        
        Iterator() = default;
        
        Iterator(OuterIterator current, OuterIterator end)
            : current_(current), end_(end) {}
        
        FlattenedMaskItem operator*() const {
            auto const& pair = *current_;
            return FlattenedMaskItem{
                .time = pair.time,
                .masks = pair.masks
            };
        }
        
        Iterator& operator++() {
            ++current_;
            return *this;
        }
        
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(Iterator const& other) const {
            return current_ == other.current_;
        }
        
        bool operator!=(Iterator const& other) const {
            return !(*this == other);
        }
        
    private:
        OuterIterator current_;
        OuterIterator end_;
    };
    
    explicit FlattenedMaskAdapter(Range range)
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
 * @brief Helper function to create a flattened adapter for MaskData ranges
 * 
 * This factory function deduces template parameters automatically and creates
 * a FlattenedMaskAdapter suitable for use with MaskData's getAllAsRange().
 * 
 * @tparam Range The range type (auto-deduced)
 * @param range The input range from getAllAsRange()
 * @return A FlattenedMaskAdapter that yields FlattenedMaskItem objects
 * 
 * Example:
 * @code
 * auto mask_range = mask_data->getAllAsRange();
 * auto data_source = flattenMaskData(mask_range);
 * 
 * MaskParticleFilter tracker(...);
 * tracker.track(data_source, ...);
 * @endcode
 */
template<typename Range>
auto flattenMaskData(Range&& range) {
    return FlattenedMaskAdapter<std::remove_reference_t<Range>>(std::forward<Range>(range));
}

} // namespace StateEstimation

// Enable view semantics for FlattenedMaskAdapter
template<typename Range>
inline constexpr bool std::ranges::enable_view<StateEstimation::FlattenedMaskAdapter<Range>> = true;

#endif // STATE_ESTIMATION_MASK_DATA_ADAPTER_HPP
