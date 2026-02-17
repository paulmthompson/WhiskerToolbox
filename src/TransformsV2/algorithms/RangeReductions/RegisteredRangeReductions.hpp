#ifndef WHISKERTOOLBOX_V2_REGISTERED_RANGE_REDUCTIONS_HPP
#define WHISKERTOOLBOX_V2_REGISTERED_RANGE_REDUCTIONS_HPP

/**
 * @file RegisteredRangeReductions.hpp
 * @brief Central registration for all range reduction operations
 *
 * This file provides the entry point for registering all built-in range
 * reductions with the RangeReductionRegistry singleton.
 *
 * ## Automatic Registration
 *
 * Range reductions are automatically registered at static initialization time.
 * Simply including the header or linking the translation unit will trigger
 * registration.
 *
 * ## Manual Registration
 *
 * If you need explicit control over registration timing (e.g., for testing),
 * call `registerAllRangeReductions()` or the individual registration functions.
 *
 * ## Adding New Reductions
 *
 * 1. Create your reduction function in EventRangeReductions.hpp or
 *    ValueRangeReductions.hpp (or a new file for a new element type)
 * 2. Add registration code in RegisteredRangeReductions.cpp
 * 3. Add the source file to CMakeLists.txt
 *
 * @see EventRangeReductions.hpp for event-based reductions
 * @see ValueRangeReductions.hpp for value-based reductions
 * @see RangeReductionRegistry.hpp for the registry infrastructure
 */

namespace WhiskerToolbox::Transforms::V2::RangeReductions {

/**
 * @brief Register all built-in range reductions
 *
 * This is called automatically at static initialization time.
 * You typically don't need to call this manually unless you're
 * working with a custom registry instance.
 */
void registerAllRangeReductions();

/**
 * @brief Register event-based range reductions (EventWithId)
 *
 * Registers:
 * - EventCount: Count total events
 * - FirstPositiveLatency: Time of first event after t=0
 * - LastNegativeLatency: Time of last event before t=0
 * - EventCountInWindow: Count events in time window
 * - MeanInterEventInterval: Mean interval between events
 * - EventTimeSpan: Duration from first to last event
 */
void registerEventRangeReductions();

/**
 * @brief Register value-based range reductions (TimeValuePoint)
 *
 * Registers:
 * - MaxValue: Maximum value
 * - MinValue: Minimum value
 * - MeanValue: Mean value
 * - StdValue: Standard deviation
 * - TimeOfMax: Time of maximum value
 * - TimeOfMin: Time of minimum value
 * - TimeOfThresholdCross: First time crossing threshold
 * - SumValue: Sum of values
 * - ValueRange: Max - Min
 * - AreaUnderCurve: Trapezoidal integration
 * - CountAboveThreshold: Samples above threshold
 * - FractionAboveThreshold: Fraction above threshold
 */
void registerValueRangeReductions();

}// namespace WhiskerToolbox::Transforms::V2::RangeReductions

#endif// WHISKERTOOLBOX_V2_REGISTERED_RANGE_REDUCTIONS_HPP
