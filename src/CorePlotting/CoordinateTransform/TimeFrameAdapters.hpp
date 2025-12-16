#ifndef COREPLOTTING_COORDINATETRANSFORM_TIMEFRAMEADAPTERS_HPP
#define COREPLOTTING_COORDINATETRANSFORM_TIMEFRAMEADAPTERS_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>

namespace CorePlotting {

// ============================================================================
// Type Traits for Data Types
// ============================================================================

/**
 * @brief Concept for types that have a TimeFrameIndex as first element of a pair
 * 
 * Matches: std::pair<TimeFrameIndex, T>, std::tuple<TimeFrameIndex, T>
 */
template<typename T>
concept TimeIndexValuePair = requires(T t) {
    { std::get<0>(t) } -> std::convertible_to<TimeFrameIndex>;
    { std::get<1>(t) };
};

/**
 * @brief Concept for EventWithId-like types (has event_time member)
 */
template<typename T>
concept HasEventTime = requires(T t) {
    { t.event_time };
    requires std::same_as<std::remove_cvref_t<decltype(t.event_time)>, TimeFrameIndex>;
};

/**
 * @brief Concept for IntervalWithId-like types (has interval member with start/end)
 */
template<typename T>
concept HasInterval = requires(T t) {
    { t.interval.start } -> std::convertible_to<int64_t>;
    { t.interval.end } -> std::convertible_to<int64_t>;
};

/**
 * @brief Concept for bare Interval types (has start/end directly as int64_t)
 * 
 * Note: Interval uses int64_t for start/end, which represent TimeFrameIndex values
 * This concept excludes types that have an 'interval' member (IntervalWithId)
 */
template<typename T>
concept IsInterval = requires(T t) {
    { t.start } -> std::convertible_to<int64_t>;
    { t.end } -> std::convertible_to<int64_t>;
} && !requires(T t) {
    t.interval;// Excludes IntervalWithId
} && !requires(T t) {
    t.event_time;// Excludes EventWithId
};

// ============================================================================
// Result Types for Transformed Data
// ============================================================================

/**
 * @brief Result of transforming a time-value pair to absolute time
 * 
 * Preserves the value type from the original pair.
 */
template<typename ValueType>
struct AbsoluteTimeValue {
    int time;       ///< Absolute time from TimeFrame
    ValueType value;///< Original value (unchanged)

    AbsoluteTimeValue(int t, ValueType v)
        : time(t),
          value(std::move(v)) {}
};

/**
 * @brief Result of transforming an event to absolute time
 */
struct AbsoluteTimeEvent {
    int time;          ///< Absolute time from TimeFrame
    EntityId entity_id;///< Entity ID (if present)

    AbsoluteTimeEvent(int t, EntityId id = EntityId{0})
        : time(t),
          entity_id(id) {}
};

/**
 * @brief Result of transforming an interval to absolute time
 */
struct AbsoluteTimeInterval {
    int start;         ///< Absolute start time
    int end;           ///< Absolute end time
    EntityId entity_id;///< Entity ID (if present)

    AbsoluteTimeInterval(int s, int e, EntityId id = EntityId{0})
        : start(s),
          end(e),
          entity_id(id) {}
};

// ============================================================================
// Forward Transform: TimeFrameIndex → Absolute Time
// ============================================================================

/**
 * @brief Range adapter closure that transforms TimeFrameIndex to absolute time
 * 
 * This adapter can be used with the pipe operator to transform ranges of
 * time-indexed data into ranges with absolute time coordinates.
 * 
 * Supported input types:
 * - std::pair<TimeFrameIndex, T> → AbsoluteTimeValue<T>
 * - TimeFrameIndex → int
 * - EventWithId (has .event_time) → AbsoluteTimeEvent
 * - IntervalWithId (has .interval) → AbsoluteTimeInterval
 * - Interval (has .start/.end) → AbsoluteTimeInterval
 * 
 * @example
 * ```cpp
 * // AnalogTimeSeries: pair<TimeFrameIndex, float> → AbsoluteTimeValue<float>
 * for (auto [time, value] : series.getTimeValueRangeInTimeFrameIndexRange(start, end)
 *                           | toAbsoluteTime(series.getTimeFrame().get())) {
 *     vertices.push_back(static_cast<float>(time));
 *     vertices.push_back(value);
 * }
 * 
 * // DigitalEventSeries: TimeFrameIndex → int
 * for (int abs_time : events.getEventsInRange(start, end)
 *                     | toAbsoluteTime(events.getTimeFrame().get())) {
 *     drawEventAt(abs_time);
 * }
 * ```
 */
class ToAbsoluteTimeAdapter {
public:
    explicit ToAbsoluteTimeAdapter(TimeFrame const * tf)
        : _time_frame(tf) {}

    /**
     * @brief Transform a pair<TimeFrameIndex, T> to AbsoluteTimeValue<T>
     */
    template<typename T>
        requires TimeIndexValuePair<T>
    auto operator()(T const & item) const {
        auto const & [idx, value] = item;
        int const abs_time = _time_frame->getTimeAtIndex(idx);
        return AbsoluteTimeValue<std::decay_t<decltype(value)>>{abs_time, value};
    }

    /**
     * @brief Transform bare TimeFrameIndex to int
     */
    int operator()(TimeFrameIndex const & item) const {
        return _time_frame->getTimeAtIndex(item);
    }

    /**
     * @brief Transform EventWithId-like types to AbsoluteTimeEvent
     */
    template<typename T>
        requires HasEventTime<T>
    auto operator()(T const & item) const {
        int const abs_time = _time_frame->getTimeAtIndex(item.event_time);
        if constexpr (requires { item.entity_id; }) {
            return AbsoluteTimeEvent{abs_time, item.entity_id};
        } else {
            return AbsoluteTimeEvent{abs_time};
        }
    }

    /**
     * @brief Transform IntervalWithId-like types to AbsoluteTimeInterval
     */
    template<typename T>
        requires HasInterval<T>
    auto operator()(T const & item) const {
        int const abs_start = _time_frame->getTimeAtIndex(TimeFrameIndex{item.interval.start});
        int const abs_end = _time_frame->getTimeAtIndex(TimeFrameIndex{item.interval.end});
        if constexpr (requires { item.entity_id; }) {
            return AbsoluteTimeInterval{abs_start, abs_end, item.entity_id};
        } else {
            return AbsoluteTimeInterval{abs_start, abs_end};
        }
    }

    /**
     * @brief Transform Interval (int64_t start/end) to AbsoluteTimeInterval
     * 
     * Non-template overload for the concrete Interval type
     */
    AbsoluteTimeInterval operator()(Interval const & item) const {
        int const abs_start = _time_frame->getTimeAtIndex(TimeFrameIndex{item.start});
        int const abs_end = _time_frame->getTimeAtIndex(TimeFrameIndex{item.end});
        return AbsoluteTimeInterval{abs_start, abs_end};
    }

    /**
     * @brief Transform TimeFrameInterval to AbsoluteTimeInterval
     */
    AbsoluteTimeInterval operator()(TimeFrameInterval const & item) const {
        int const abs_start = _time_frame->getTimeAtIndex(item.start);
        int const abs_end = _time_frame->getTimeAtIndex(item.end);
        return AbsoluteTimeInterval{abs_start, abs_end};
    }

    /**
     * @brief Pipe operator support for TimeFrameIndex ranges
     */
    template<std::ranges::viewable_range R>
        requires std::same_as<std::ranges::range_value_t<R>, TimeFrameIndex>
    friend auto operator|(R && range, ToAbsoluteTimeAdapter const & adapter) {
        return std::forward<R>(range) | std::views::transform([tf = adapter._time_frame](TimeFrameIndex const & idx) {
                   return tf->getTimeAtIndex(idx);
               });
    }

    /**
     * @brief Pipe operator support for pair<TimeFrameIndex, T> ranges
     */
    template<std::ranges::viewable_range R>
        requires TimeIndexValuePair<std::ranges::range_value_t<R>>
    friend auto operator|(R && range, ToAbsoluteTimeAdapter const & adapter) {
        return std::forward<R>(range) | std::views::transform([tf = adapter._time_frame](auto const & item) {
                   auto const & [idx, value] = item;
                   int const abs_time = tf->getTimeAtIndex(idx);
                   return AbsoluteTimeValue<std::decay_t<decltype(value)>>{abs_time, value};
               });
    }

    /**
     * @brief Pipe operator support for Interval ranges
     */
    template<std::ranges::viewable_range R>
        requires std::same_as<std::ranges::range_value_t<R>, Interval>
    friend auto operator|(R && range, ToAbsoluteTimeAdapter const & adapter) {
        return std::forward<R>(range) | std::views::transform([tf = adapter._time_frame](Interval const & item) {
                   int const abs_start = tf->getTimeAtIndex(TimeFrameIndex{item.start});
                   int const abs_end = tf->getTimeAtIndex(TimeFrameIndex{item.end});
                   return AbsoluteTimeInterval{abs_start, abs_end};
               });
    }

    /**
     * @brief Pipe operator support for TimeFrameInterval ranges
     */
    template<std::ranges::viewable_range R>
        requires std::same_as<std::ranges::range_value_t<R>, TimeFrameInterval>
    friend auto operator|(R && range, ToAbsoluteTimeAdapter const & adapter) {
        return std::forward<R>(range) | std::views::transform([tf = adapter._time_frame](TimeFrameInterval const & item) {
                   int const abs_start = tf->getTimeAtIndex(item.start);
                   int const abs_end = tf->getTimeAtIndex(item.end);
                   return AbsoluteTimeInterval{abs_start, abs_end};
               });
    }

    /**
     * @brief Pipe operator support for EventWithId-like ranges
     */
    template<std::ranges::viewable_range R>
        requires HasEventTime<std::ranges::range_value_t<R>>
    friend auto operator|(R && range, ToAbsoluteTimeAdapter const & adapter) {
        return std::forward<R>(range) | std::views::transform([tf = adapter._time_frame](auto const & item) {
                   int const abs_time = tf->getTimeAtIndex(item.event_time);
                   if constexpr (requires { item.entity_id; }) {
                       return AbsoluteTimeEvent{abs_time, item.entity_id};
                   } else {
                       return AbsoluteTimeEvent{abs_time};
                   }
               });
    }

    /**
     * @brief Pipe operator support for IntervalWithId-like ranges
     */
    template<std::ranges::viewable_range R>
        requires HasInterval<std::ranges::range_value_t<R>>
    friend auto operator|(R && range, ToAbsoluteTimeAdapter const & adapter) {
        return std::forward<R>(range) | std::views::transform([tf = adapter._time_frame](auto const & item) {
                   int const abs_start = tf->getTimeAtIndex(TimeFrameIndex{item.interval.start});
                   int const abs_end = tf->getTimeAtIndex(TimeFrameIndex{item.interval.end});
                   if constexpr (requires { item.entity_id; }) {
                       return AbsoluteTimeInterval{abs_start, abs_end, item.entity_id};
                   } else {
                       return AbsoluteTimeInterval{abs_start, abs_end};
                   }
               });
    }

private:
    TimeFrame const * _time_frame;
};

/**
 * @brief Factory function for creating ToAbsoluteTimeAdapter
 * 
 * @param tf TimeFrame to use for conversion (must remain valid during iteration)
 * @return ToAbsoluteTimeAdapter that can be piped with ranges
 * 
 * @example
 * ```cpp
 * auto adapter = toAbsoluteTime(series.getTimeFrame().get());
 * for (auto [time, value] : series.getAllSamples() | adapter) {
 *     // time is now absolute time (int)
 * }
 * ```
 */
inline ToAbsoluteTimeAdapter toAbsoluteTime(TimeFrame const * tf) {
    return ToAbsoluteTimeAdapter{tf};
}

// ============================================================================
// Inverse Transform: Absolute Time → TimeFrameIndex
// ============================================================================

/**
 * @brief Convert absolute time to TimeFrameIndex
 * 
 * This is the inverse of toAbsoluteTime, useful for:
 * - Mouse hover: screen X → absolute time → TimeFrameIndex
 * - Spatial queries: finding what data exists at a given display position
 * 
 * @param absolute_time Time value in absolute units
 * @param tf TimeFrame to use for conversion
 * @param preceding If true, return the index of the preceding frame (default)
 * @return TimeFrameIndex corresponding to the absolute time
 * 
 * @example
 * ```cpp
 * // In mouse move handler:
 * float screen_x = event->pos().x();
 * float absolute_time = screenXToTime(screen_x);  // from your widget
 * TimeFrameIndex idx = toTimeFrameIndex(absolute_time, master_time_frame);
 * // Now idx can be used to query data or display to user
 * ```
 */
inline TimeFrameIndex toTimeFrameIndex(float absolute_time, TimeFrame const * tf, bool preceding = true) {
    return tf->getIndexAtTime(absolute_time, preceding);
}

inline TimeFrameIndex toTimeFrameIndex(int absolute_time, TimeFrame const * tf, bool preceding = true) {
    return tf->getIndexAtTime(static_cast<float>(absolute_time), preceding);
}

// ============================================================================
// TimeFrame Converter Context
// ============================================================================

/**
 * @brief Context object for bidirectional TimeFrame conversions
 * 
 * Holds a reference to a TimeFrame and provides both forward and inverse
 * conversion methods. Useful when you need to perform multiple conversions
 * with the same TimeFrame.
 * 
 * @example
 * ```cpp
 * TimeFrameConverter converter(master_time_frame.get());
 * 
 * // Forward: get absolute time for display
 * int display_time = converter.toAbsolute(data_index);
 * 
 * // Inverse: get index from mouse position
 * TimeFrameIndex hover_idx = converter.toIndex(mouse_time);
 * 
 * // Range adapter
 * for (auto [time, val] : series.getAllSamples() | converter.adapter()) {
 *     // ...
 * }
 * ```
 */
class TimeFrameConverter {
public:
    explicit TimeFrameConverter(TimeFrame const * tf)
        : _time_frame(tf) {}

    /**
     * @brief Convert TimeFrameIndex to absolute time
     */
    [[nodiscard]] int toAbsolute(TimeFrameIndex idx) const {
        return _time_frame->getTimeAtIndex(idx);
    }

    /**
     * @brief Convert absolute time to TimeFrameIndex
     */
    [[nodiscard]] TimeFrameIndex toIndex(float absolute_time, bool preceding = true) const {
        return _time_frame->getIndexAtTime(absolute_time, preceding);
    }

    [[nodiscard]] TimeFrameIndex toIndex(int absolute_time, bool preceding = true) const {
        return _time_frame->getIndexAtTime(static_cast<float>(absolute_time), preceding);
    }

    /**
     * @brief Get a range adapter for this converter
     */
    [[nodiscard]] ToAbsoluteTimeAdapter adapter() const {
        return ToAbsoluteTimeAdapter{_time_frame};
    }

    /**
     * @brief Get the underlying TimeFrame
     */
    [[nodiscard]] TimeFrame const * timeFrame() const { return _time_frame; }

private:
    TimeFrame const * _time_frame;
};

// ============================================================================
// Cross-TimeFrame Conversion
// ============================================================================

/**
 * @brief Convert a TimeFrameIndex from one TimeFrame to another
 * 
 * This is essential for aligning data from different sources (e.g., neural data
 * and video frames) that may have different sampling rates.
 * 
 * The conversion goes through absolute time:
 *   source_index → absolute_time → target_index
 * 
 * @param source_index Index in source TimeFrame
 * @param source_tf Source TimeFrame
 * @param target_tf Target TimeFrame
 * @param preceding If true, return preceding frame when exact match not found
 * @return TimeFrameIndex in target TimeFrame coordinate system
 * 
 * @example
 * ```cpp
 * // Convert from neural data time frame to video time frame
 * TimeFrameIndex neural_idx{1000};
 * TimeFrameIndex video_idx = convertTimeFrameIndex(
 *     neural_idx, neural_tf, video_tf);
 * ```
 */
inline TimeFrameIndex convertTimeFrameIndex(
        TimeFrameIndex source_index,
        TimeFrame const * source_tf,
        TimeFrame const * target_tf,
        bool preceding = true) {

    // Optimization: if same TimeFrame, return directly
    if (source_tf == target_tf) {
        return source_index;
    }

    // Convert through absolute time
    int const absolute_time = source_tf->getTimeAtIndex(source_index);
    return target_tf->getIndexAtTime(static_cast<float>(absolute_time), preceding);
}

/**
 * @brief Range adapter for cross-TimeFrame conversion
 * 
 * Transforms TimeFrameIndex values from source to target TimeFrame coordinates.
 * Useful for aligning data from one series to another's time base.
 */
class ToTargetFrameAdapter {
public:
    ToTargetFrameAdapter(TimeFrame const * source_tf, TimeFrame const * target_tf)
        : _source_tf(source_tf),
          _target_tf(target_tf) {}

    /**
     * @brief Transform a single TimeFrameIndex
     */
    TimeFrameIndex operator()(TimeFrameIndex idx) const {
        return convertTimeFrameIndex(idx, _source_tf, _target_tf);
    }

    /**
     * @brief Transform a time-value pair (converts the time, preserves the value)
     */
    template<typename T>
        requires TimeIndexValuePair<T>
    auto operator()(T const & item) const {
        auto const & [idx, value] = item;
        TimeFrameIndex const target_idx = convertTimeFrameIndex(idx, _source_tf, _target_tf);
        return std::pair{target_idx, value};
    }

    /**
     * @brief Pipe operator support
     */
    template<std::ranges::viewable_range R>
    friend auto operator|(R && range, ToTargetFrameAdapter const & adapter) {
        return std::forward<R>(range) | std::views::transform([adapter](auto const & item) {
                   return adapter(item);
               });
    }

private:
    TimeFrame const * _source_tf;
    TimeFrame const * _target_tf;
};

/**
 * @brief Factory function for creating ToTargetFrameAdapter
 * 
 * @param source_tf TimeFrame of the input data
 * @param target_tf TimeFrame to convert to (e.g., master TimeFrame)
 * @return ToTargetFrameAdapter that can be piped with ranges
 * 
 * @example
 * ```cpp
 * // Convert analog series indices to master time frame
 * auto series_tf = series.getTimeFrame().get();
 * for (auto [master_idx, value] : series.getAllSamples() 
 *                                 | toTargetFrame(series_tf, master_tf)) {
 *     // master_idx is now in master TimeFrame coordinates
 * }
 * ```
 */
inline ToTargetFrameAdapter toTargetFrame(TimeFrame const * source_tf, TimeFrame const * target_tf) {
    return ToTargetFrameAdapter{source_tf, target_tf};
}

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_TIMEFRAMEADAPTERS_HPP
