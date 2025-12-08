#ifndef DIGITAL_INTERVAL_BOOLEAN_SCENARIOS_HPP
#define DIGITAL_INTERVAL_BOOLEAN_SCENARIOS_HPP

#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"
#include <memory>
#include <utility>

/**
 * @brief Boolean operation test scenarios for DigitalIntervalSeries
 * 
 * This namespace contains pre-configured test data for boolean operations
 * on digital interval series. These scenarios are extracted from
 * DigitalIntervalBooleanTestFixture to enable reuse across v1 and v2 tests
 * without the heavy DataManager dependency.
 * 
 * Each scenario returns a pair of interval series (input, other) for testing
 * binary operations (AND, OR, XOR, AND_NOT). For unary operations (NOT),
 * only the first series is used.
 */
namespace digital_interval_boolean_scenarios {

/**
 * @brief Pair of DigitalIntervalSeries for binary operations
 */
using IntervalSeriesPair = std::pair<
    std::shared_ptr<DigitalIntervalSeries>,
    std::shared_ptr<DigitalIntervalSeries>
>;

// ============================================================================
// AND Operation Test Data
// ============================================================================

/**
 * @brief Basic overlapping intervals for AND operation
 * 
 * Input: (1,5), (10,15)
 * Other: (3,7), (12,20)
 * AND Expected: (3,5), (12,15)
 */
inline IntervalSeriesPair and_overlapping() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .withInterval(10, 15)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(3, 7)
        .withInterval(12, 20)
        .build();
    
    return {input, other};
}

/**
 * @brief No overlap between intervals for AND operation
 * 
 * Input: (1,5)
 * Other: (10,15)
 * AND Expected: empty
 */
inline IntervalSeriesPair and_no_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(10, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief Complete overlap (identical intervals) for AND operation
 * 
 * Input: (1,10)
 * Other: (1,10)
 * AND Expected: (1,10)
 */
inline IntervalSeriesPair and_complete_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    return {input, other};
}

/**
 * @brief One series is subset of other for AND operation
 * 
 * Input: (5,15)
 * Other: (1,20)
 * AND Expected: (5,15)
 */
inline IntervalSeriesPair and_subset() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(5, 15)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(1, 20)
        .build();
    
    return {input, other};
}

// ============================================================================
// OR Operation Test Data
// ============================================================================

/**
 * @brief Separate intervals (no overlap) for OR operation
 * 
 * Input: (1,5)
 * Other: (10,15)
 * OR Expected: (1,5), (10,15)
 */
inline IntervalSeriesPair or_separate() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(10, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief Overlapping intervals that should merge for OR operation
 * 
 * Input: (1,10)
 * Other: (5,15)
 * OR Expected: (1,15)
 */
inline IntervalSeriesPair or_overlapping_merge() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(5, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief Multiple intervals with gaps for OR operation
 * 
 * Input: (1,5), (15,20)
 * Other: (8,12), (18,25)
 * OR Expected: (1,5), (8,12), (15,25)
 */
inline IntervalSeriesPair or_multiple_with_gaps() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .withInterval(15, 20)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(8, 12)
        .withInterval(18, 25)
        .build();
    
    return {input, other};
}

// ============================================================================
// XOR Operation Test Data
// ============================================================================

/**
 * @brief No overlap (same as OR) for XOR operation
 * 
 * Input: (1,5)
 * Other: (10,15)
 * XOR Expected: (1,5), (10,15)
 */
inline IntervalSeriesPair xor_no_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(10, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief Partial overlap (excludes overlapping part) for XOR operation
 * 
 * Input: (1,10)
 * Other: (5,15)
 * XOR Expected: (1,4), (11,15)
 */
inline IntervalSeriesPair xor_partial_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(5, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief Complete overlap (results in nothing) for XOR operation
 * 
 * Input: (1,10)
 * Other: (1,10)
 * XOR Expected: empty
 */
inline IntervalSeriesPair xor_complete_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    return {input, other};
}

/**
 * @brief Complex pattern with multiple intervals for XOR operation
 * 
 * Input: (1,5), (10,15)
 * Other: (3,12)
 * XOR Expected: (1,2), (6,9), (13,15)
 */
inline IntervalSeriesPair xor_complex() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .withInterval(10, 15)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(3, 12)
        .build();
    
    return {input, other};
}

// ============================================================================
// NOT Operation Test Data (only uses input, ignores other)
// ============================================================================

/**
 * @brief Single interval for NOT operation
 * 
 * Input: (5,10)
 * NOT Expected: empty (entire range is covered)
 */
inline std::shared_ptr<DigitalIntervalSeries> not_single_interval() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(5, 10)
        .build();
}

/**
 * @brief Intervals with gaps for NOT operation
 * 
 * Input: (1,5), (10,15)
 * NOT Expected: (6,9) - the gap between intervals
 */
inline std::shared_ptr<DigitalIntervalSeries> not_with_gaps() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .withInterval(10, 15)
        .build();
}

/**
 * @brief Multiple gaps for NOT operation
 * 
 * Input: (1,3), (5,7), (9,11)
 * NOT Expected: (4,4), (8,8)
 */
inline std::shared_ptr<DigitalIntervalSeries> not_multiple_gaps() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(1, 3)
        .withInterval(5, 7)
        .withInterval(9, 11)
        .build();
}

// ============================================================================
// AND_NOT Operation Test Data
// ============================================================================

/**
 * @brief Subtract overlapping portion for AND_NOT operation
 * 
 * Input: (1,10)
 * Other: (5,15)
 * AND_NOT Expected: (1,4)
 */
inline IntervalSeriesPair and_not_subtract_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(5, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief No overlap (input unchanged) for AND_NOT operation
 * 
 * Input: (1,5)
 * Other: (10,15)
 * AND_NOT Expected: (1,5)
 */
inline IntervalSeriesPair and_not_no_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 5)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(10, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief Complete overlap removes everything for AND_NOT operation
 * 
 * Input: (5,10)
 * Other: (1,15)
 * AND_NOT Expected: empty
 */
inline IntervalSeriesPair and_not_complete_overlap() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(5, 10)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(1, 15)
        .build();
    
    return {input, other};
}

/**
 * @brief Punch holes in input for AND_NOT operation
 * 
 * Input: (1,20)
 * Other: (5,8), (12,15)
 * AND_NOT Expected: (1,4), (9,11), (16,20)
 */
inline IntervalSeriesPair and_not_punch_holes() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 20)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(5, 8)
        .withInterval(12, 15)
        .build();
    
    return {input, other};
}

// ============================================================================
// Edge Cases
// ============================================================================

/**
 * @brief Empty input series
 * 
 * Input: empty
 * Other: (1,10)
 */
inline IntervalSeriesPair empty_input() {
    auto input = DigitalIntervalSeriesBuilder().build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(1, 10)
        .build();
    
    return {input, other};
}

/**
 * @brief Both series empty
 * 
 * Input: empty
 * Other: empty
 */
inline IntervalSeriesPair both_empty() {
    auto input = DigitalIntervalSeriesBuilder().build();
    auto other = DigitalIntervalSeriesBuilder().build();
    
    return {input, other};
}

/**
 * @brief Empty for NOT operation
 * 
 * Input: empty
 * NOT Expected: empty
 */
inline std::shared_ptr<DigitalIntervalSeries> not_empty() {
    return DigitalIntervalSeriesBuilder().build();
}

/**
 * @brief Large intervals for progress callback testing
 * 
 * Input: (1,100)
 * Other: (50,150)
 */
inline IntervalSeriesPair large_intervals() {
    auto input = DigitalIntervalSeriesBuilder()
        .withInterval(1, 100)
        .build();
    
    auto other = DigitalIntervalSeriesBuilder()
        .withInterval(50, 150)
        .build();
    
    return {input, other};
}

} // namespace digital_interval_boolean_scenarios

#endif // DIGITAL_INTERVAL_BOOLEAN_SCENARIOS_HPP
