#ifndef WHISKERTOOLBOX_INTERVAL_ADAPTERS_HPP
#define WHISKERTOOLBOX_INTERVAL_ADAPTERS_HPP

/**
 * @file IntervalAdapters.hpp
 * @brief Lazy interval adapters for flexible interval sources in GatherResult
 *
 * Provides adapters that allow GatherResult to work with:
 * - DigitalEventSeries expanded to intervals (each event ± window)
 * - DigitalIntervalSeries with custom alignment points (start/end/center)
 *
 * All adapters are lazy - they don't materialize intervals until iterated.
 *
 * @example
 * ```cpp
 * // Expand events to intervals with ±50 window
 * auto events = std::make_shared<DigitalEventSeries>(...);
 * EventExpanderAdapter adapter(events, 50, 50);
 * auto result = gather(source_data, adapter);
 *
 * // Use interval starts as alignment, but keep full interval for gathering
 * auto intervals = std::make_shared<DigitalIntervalSeries>(...);
 * IntervalWithAlignmentAdapter adapter(intervals, AlignmentPoint::Start);
 * auto result = gather(source_data, adapter);
 * ```
 */

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <cstdint>
#include <iterator>
#include <memory>

namespace WhiskerToolbox::Transforms::V2 {

// =============================================================================
// Interval Element Types
// =============================================================================

/**
 * @brief An interval with an associated alignment time
 *
 * Used by adapters to specify both the interval bounds (for data gathering)
 * and the alignment time (for time normalization in projections).
 */
struct AlignedInterval {
    int64_t start;          ///< Interval start (inclusive)
    int64_t end;            ///< Interval end (exclusive)
    int64_t alignment_time; ///< Time to use for alignment in projections

    [[nodiscard]] constexpr int64_t duration() const noexcept {
        return end - start;
    }
};

/**
 * @brief Alignment point options for intervals
 */
enum class AlignmentPoint {
    Start,  ///< Use interval start as alignment time
    End,    ///< Use interval end as alignment time
    Center  ///< Use interval center as alignment time
};

// =============================================================================
// Concepts for Interval Sources
// =============================================================================

/**
 * @brief Concept for types that can provide AlignedInterval elements
 *
 * Any type satisfying this concept can be used with gather().
 */
template<typename T>
concept IntervalSource = requires(T const& t) {
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
    { t.size() } -> std::convertible_to<std::size_t>;
};

// =============================================================================
// EventExpanderAdapter - DigitalEventSeries → Intervals
// =============================================================================

/**
 * @brief Lazily expands a DigitalEventSeries into intervals
 *
 * Each event at time T becomes an interval [T - pre_window, T + post_window)
 * with alignment_time = T.
 *
 * This is a view adapter - it doesn't copy the underlying data.
 */
class EventExpanderAdapter {
public:
    /**
     * @brief Iterator that lazily produces AlignedInterval from events
     * 
     * Note: Each dereference iterates through view() to find the element.
     * For GatherResult construction (single forward pass), this is efficient.
     */
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = AlignedInterval;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type const*;
        using reference = value_type;

        iterator() = default;

        iterator(
            DigitalEventSeries const* events,
            std::size_t index,
            int64_t pre_window,
            int64_t post_window)
            : _events(events)
            , _index(index)
            , _pre_window(pre_window)
            , _post_window(post_window) {}

        [[nodiscard]] value_type operator*() const {
            // Iterate to find element at current index
            std::size_t idx = 0;
            for (auto const& event : _events->view()) {
                if (idx == _index) {
                    int64_t time = event.time().getValue();
                    return AlignedInterval{
                        .start = time - _pre_window,
                        .end = time + _post_window,
                        .alignment_time = time
                    };
                }
                ++idx;
            }
            // Should never reach here if iterator is valid
            return AlignedInterval{0, 0, 0};
        }

        iterator& operator++() {
            ++_index;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(iterator const& other) const {
            return _index == other._index;
        }

        [[nodiscard]] bool operator!=(iterator const& other) const {
            return !(*this == other);
        }

    private:
        DigitalEventSeries const* _events{nullptr};
        std::size_t _index{0};
        int64_t _pre_window{0};
        int64_t _post_window{0};
    };

    /**
     * @brief Construct adapter from event series and window parameters
     *
     * @param events The event series to expand
     * @param pre_window Time before each event to include
     * @param post_window Time after each event to include
     */
    EventExpanderAdapter(
        std::shared_ptr<DigitalEventSeries const> events,
        int64_t pre_window,
        int64_t post_window)
        : _events(std::move(events))
        , _pre_window(pre_window)
        , _post_window(post_window) {}

    /**
     * @brief Construct with symmetric window
     */
    EventExpanderAdapter(
        std::shared_ptr<DigitalEventSeries const> events,
        int64_t window)
        : EventExpanderAdapter(std::move(events), window, window) {}

    [[nodiscard]] iterator begin() const {
        return iterator(_events.get(), 0, _pre_window, _post_window);
    }

    [[nodiscard]] iterator end() const {
        return iterator(_events.get(), _events->size(), _pre_window, _post_window);
    }

    [[nodiscard]] std::size_t size() const {
        return _events->size();
    }

    /**
     * @brief Get the underlying event series
     */
    [[nodiscard]] std::shared_ptr<DigitalEventSeries const> const& events() const {
        return _events;
    }

    [[nodiscard]] int64_t pre_window() const { return _pre_window; }
    [[nodiscard]] int64_t post_window() const { return _post_window; }

private:
    std::shared_ptr<DigitalEventSeries const> _events;
    int64_t _pre_window;
    int64_t _post_window;
};

// =============================================================================
// IntervalWithAlignmentAdapter - DigitalIntervalSeries with custom alignment
// =============================================================================

/**
 * @brief Adapter that provides intervals with custom alignment points
 *
 * Wraps a DigitalIntervalSeries and allows specifying whether alignment
 * should be at the start, end, or center of each interval.
 *
 * This is useful when you have intervals but want to align data relative
 * to a specific point within each interval.
 */
class IntervalWithAlignmentAdapter {
public:
    /**
     * @brief Iterator that produces AlignedInterval with custom alignment
     * 
     * Note: Each dereference iterates through view() to find the element.
     * For GatherResult construction (single forward pass), this is efficient.
     */
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = AlignedInterval;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type const*;
        using reference = value_type;

        iterator() = default;

        iterator(
            DigitalIntervalSeries const* intervals,
            std::size_t index,
            AlignmentPoint align)
            : _intervals(intervals)
            , _index(index)
            , _align(align) {}

        [[nodiscard]] value_type operator*() const {
            // Iterate to find element at current index
            std::size_t idx = 0;
            for (auto const& interval_with_id : _intervals->view()) {
                if (idx == _index) {
                    auto const& interval = interval_with_id.interval;

                    int64_t alignment_time;
                    switch (_align) {
                        case AlignmentPoint::Start:
                            alignment_time = interval.start;
                            break;
                        case AlignmentPoint::End:
                            alignment_time = interval.end;
                            break;
                        case AlignmentPoint::Center:
                            alignment_time = (interval.start + interval.end) / 2;
                            break;
                    }

                    return AlignedInterval{
                        .start = interval.start,
                        .end = interval.end,
                        .alignment_time = alignment_time
                    };
                }
                ++idx;
            }
            return AlignedInterval{0, 0, 0};
        }

        iterator& operator++() {
            ++_index;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(iterator const& other) const {
            return _index == other._index;
        }

        [[nodiscard]] bool operator!=(iterator const& other) const {
            return !(*this == other);
        }

    private:
        DigitalIntervalSeries const* _intervals{nullptr};
        std::size_t _index{0};
        AlignmentPoint _align{AlignmentPoint::Start};
    };

    /**
     * @brief Construct adapter from interval series and alignment point
     *
     * @param intervals The interval series
     * @param align Which point in each interval to use for alignment
     */
    IntervalWithAlignmentAdapter(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        AlignmentPoint align = AlignmentPoint::Start)
        : _intervals(std::move(intervals))
        , _align(align) {}

    [[nodiscard]] iterator begin() const {
        return iterator(_intervals.get(), 0, _align);
    }

    [[nodiscard]] iterator end() const {
        return iterator(_intervals.get(), _intervals->size(), _align);
    }

    [[nodiscard]] std::size_t size() const {
        return _intervals->size();
    }

    /**
     * @brief Get the underlying interval series
     */
    [[nodiscard]] std::shared_ptr<DigitalIntervalSeries const> const& intervals() const {
        return _intervals;
    }

    [[nodiscard]] AlignmentPoint alignment() const { return _align; }

private:
    std::shared_ptr<DigitalIntervalSeries const> _intervals;
    AlignmentPoint _align;
};

// =============================================================================
// Helper functions
// =============================================================================

/**
 * @brief Create an EventExpanderAdapter
 *
 * @param events Event series to expand
 * @param pre_window Time before each event
 * @param post_window Time after each event
 */
inline EventExpanderAdapter expandEvents(
    std::shared_ptr<DigitalEventSeries const> events,
    int64_t pre_window,
    int64_t post_window) {
    return EventExpanderAdapter(std::move(events), pre_window, post_window);
}

/**
 * @brief Create an EventExpanderAdapter with symmetric window
 */
inline EventExpanderAdapter expandEvents(
    std::shared_ptr<DigitalEventSeries const> events,
    int64_t window) {
    return EventExpanderAdapter(std::move(events), window, window);
}

/**
 * @brief Create an IntervalWithAlignmentAdapter
 *
 * @param intervals Interval series
 * @param align Alignment point (default: Start)
 */
inline IntervalWithAlignmentAdapter withAlignment(
    std::shared_ptr<DigitalIntervalSeries const> intervals,
    AlignmentPoint align = AlignmentPoint::Start) {
    return IntervalWithAlignmentAdapter(std::move(intervals), align);
}

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_INTERVAL_ADAPTERS_HPP
