/**
 * @file ClockTicksReflector.hpp
 * @brief reflect-cpp Reflector specialization for ClockTicks.
 *
 * Serializes ClockTicks as a plain int64_t for JSON round-tripping.
 * Include this header in any translation unit that needs to serialize
 * structs containing ClockTicks via rfl::json.
 */
#ifndef CLOCK_TICKS_REFLECTOR_HPP
#define CLOCK_TICKS_REFLECTOR_HPP

#include "ClockTicks.hpp"

#include <cstdint>
#include <rfl.hpp>

namespace rfl {
template<>
struct Reflector<ClockTicks> {
    using ReflType = int64_t;

    static ReflType from(ClockTicks const & t) { return t.getValue(); }
    static ClockTicks to(ReflType const & v) { return ClockTicks{v}; }
};
}// namespace rfl

#endif// CLOCK_TICKS_REFLECTOR_HPP
