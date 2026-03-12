/**
 * @file TimeFrameIndexReflector.hpp
 * @brief reflect-cpp Reflector specialization for TimeFrameIndex.
 *
 * Serializes TimeFrameIndex as a plain int64_t for JSON round-tripping.
 * Include this header in any translation unit that needs to serialize
 * structs containing TimeFrameIndex via rfl::json.
 */
#ifndef TIME_FRAME_INDEX_REFLECTOR_HPP
#define TIME_FRAME_INDEX_REFLECTOR_HPP

#include "TimeFrameIndex.hpp"

#include <rfl.hpp>

namespace rfl {
template<>
struct Reflector<TimeFrameIndex> {
    using ReflType = int64_t;
    static ReflType from(TimeFrameIndex const & t) { return t.getValue(); }
    static TimeFrameIndex to(ReflType const & v) { return TimeFrameIndex{v}; }
};
}// namespace rfl

#endif// TIME_FRAME_INDEX_REFLECTOR_HPP
