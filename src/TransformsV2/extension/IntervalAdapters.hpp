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
                    // Convert event index to absolute time if a TimeFrame exists,
                    // otherwise treat the raw index as the time value.
                    auto tf = _events->getTimeFrame();
                    int64_t abs_time = tf
                        ? tf->getTimeAtIndex(event.time())
                        : event.time().getValue();

                    // Window offsets are always in absolute time units.
                    return AlignedInterval{
                        .start = abs_time - _pre_window,
                        .end = abs_time + _post_window,
                        .alignment_time = abs_time
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

    /**
     * @brief Get the TimeFrame of the underlying event series
     *
     * Returns nullptr because the adapter's AlignedInterval values are
     * already in absolute time units (the iterator converts via the
     * event series' TimeFrame).  Returning nullptr tells GatherResult::create
     * that no further index→time conversion is needed for these values.
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const {
        return nullptr;
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
 * Two modes of operation:
 *
 * 1. **No window** (default): start/end are the raw interval bounds in TF-index
 *    space. The adapter exposes its TimeFrame so GatherResult::create can convert
 *    to absolute time.
 *
 * 2. **With window**: start/end are computed as [alignment_abs_time - pre_window,
 *    alignment_abs_time + post_window] in absolute time units. The adapter returns
 *    nullptr from getTimeFrame() so GatherResult::create treats them as already
 *    absolute (matching EventExpanderAdapter's contract).
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
            AlignmentPoint align,
            int64_t pre_window,
            int64_t post_window,
            bool use_window)
            : _intervals(intervals)
            , _index(index)
            , _align(align)
            , _pre_window(pre_window)
            , _post_window(post_window)
            , _use_window(use_window) {}

        [[nodiscard]] value_type operator*() const {
            // Iterate to find element at current index
            std::size_t idx = 0;
            for (auto const& interval_with_id : _intervals->view()) {
                if (idx == _index) {
                    auto const& interval = interval_with_id.interval;

                    int64_t alignment_idx;
                    switch (_align) {
                        case AlignmentPoint::Start:
                            alignment_idx = interval.start;
                            break;
                        case AlignmentPoint::End:
                            alignment_idx = interval.end;
                            break;
                        case AlignmentPoint::Center:
                            alignment_idx = (interval.start + interval.end) / 2;
                            break;
                    }

                    if (_use_window) {
                        // Window mode: convert alignment to absolute time,
                        // then expand by pre/post window in absolute time units.
                        auto tf = _intervals->getTimeFrame();
                        int64_t abs_time = tf
                            ? tf->getTimeAtIndex(TimeFrameIndex(alignment_idx))
                            : alignment_idx;

                        return AlignedInterval{
                            .start = abs_time - _pre_window,
                            .end = abs_time + _post_window,
                            .alignment_time = abs_time
                        };
                    } else {
                        // No window: use raw interval bounds (TF-index space)
                        return AlignedInterval{
                            .start = interval.start,
                            .end = interval.end,
                            .alignment_time = alignment_idx
                        };
                    }
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
        int64_t _pre_window{0};
        int64_t _post_window{0};
        bool _use_window{false};
    };

    /**
     * @brief Construct adapter from interval series and alignment point (no window)
     *
     * @param intervals The interval series
     * @param align Which point in each interval to use for alignment
     */
    IntervalWithAlignmentAdapter(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        AlignmentPoint align = AlignmentPoint::Start)
        : _intervals(std::move(intervals))
        , _align(align) {}

    /**
     * @brief Construct adapter with explicit window around alignment point
     *
     * When a window is specified, the iterator produces intervals in absolute
     * time: [alignment_abs - pre_window, alignment_abs + post_window].
     * The adapter returns nullptr from getTimeFrame() since values are already
     * in absolute time.
     *
     * @param intervals The interval series
     * @param align Which point in each interval to use for alignment
     * @param pre_window Absolute time units before alignment point
     * @param post_window Absolute time units after alignment point
     */
    IntervalWithAlignmentAdapter(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        AlignmentPoint align,
        int64_t pre_window,
        int64_t post_window)
        : _intervals(std::move(intervals))
        , _align(align)
        , _pre_window(pre_window)
        , _post_window(post_window)
        , _use_window(true) {}

    [[nodiscard]] iterator begin() const {
        return iterator(_intervals.get(), 0, _align,
                       _pre_window, _post_window, _use_window);
    }

    [[nodiscard]] iterator end() const {
        return iterator(_intervals.get(), _intervals->size(), _align,
                       _pre_window, _post_window, _use_window);
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

    /**
     * @brief Get the TimeFrame of the underlying interval series
     *
     * Returns nullptr when a window is specified because the iterator
     * produces absolute-time values (no further TF conversion needed).
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const {
        if (_use_window) {
            return nullptr;  // Values already in absolute time
        }
        return _intervals ? _intervals->getTimeFrame() : nullptr;
    }

    [[nodiscard]] AlignmentPoint alignment() const { return _align; }

private:
    std::shared_ptr<DigitalIntervalSeries const> _intervals;
    AlignmentPoint _align;
    int64_t _pre_window{0};
    int64_t _post_window{0};
    bool _use_window{false};
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

/**
 * @brief Create an IntervalWithAlignmentAdapter with explicit window
 *
 * The window is specified in absolute time units. The adapter produces
 * intervals centered on the alignment point: [align - pre, align + post].
 *
 * @param intervals Interval series
 * @param align Alignment point (Start, End, Center)
 * @param pre_window Absolute time units before alignment point
 * @param post_window Absolute time units after alignment point
 */
inline IntervalWithAlignmentAdapter withAlignment(
    std::shared_ptr<DigitalIntervalSeries const> intervals,
    AlignmentPoint align,
    int64_t pre_window,
    int64_t post_window) {
    return IntervalWithAlignmentAdapter(
        std::move(intervals), align, pre_window, post_window);
}

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_INTERVAL_ADAPTERS_HPP
