/**
 * @file BindingData.test.cpp
 * @brief Tests for DeepLearningBindingData utilities.
 *
 * Exercises the computeEncodingFrame() helper that combines current frame,
 * batch index, and time_offset with clamping to [0, max_frame].
 */

#include <catch2/catch_test_macros.hpp>

// Pull in the header under test — no torch or Qt dependency needed.
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"

// ════════════════════════════════════════════════════════════════════════════
// computeEncodingFrame
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("computeEncodingFrame - zero offset returns current frame",
          "[binding_data][time_offset]") {
    CHECK(computeEncodingFrame(10, 0, 0, 100) == 10);
}

TEST_CASE("computeEncodingFrame - positive offset shifts forward",
          "[binding_data][time_offset]") {
    CHECK(computeEncodingFrame(10, 0, 3, 100) == 13);
}

TEST_CASE("computeEncodingFrame - negative offset shifts backward",
          "[binding_data][time_offset]") {
    CHECK(computeEncodingFrame(10, 0, -3, 100) == 7);
}

TEST_CASE("computeEncodingFrame - batch index adds to frame",
          "[binding_data][time_offset]") {
    CHECK(computeEncodingFrame(10, 2, 0, 100) == 12);
    CHECK(computeEncodingFrame(10, 2, -1, 100) == 11);
}

TEST_CASE("computeEncodingFrame - negative offset clamps to zero",
          "[binding_data][time_offset]") {
    // current_frame=2, offset=-5 → raw=-3 → clamped to 0
    CHECK(computeEncodingFrame(2, 0, -5, 100) == 0);
    // current_frame=0, offset=-1 → raw=-1 → clamped to 0
    CHECK(computeEncodingFrame(0, 0, -1, 100) == 0);
}

TEST_CASE("computeEncodingFrame - positive offset clamps to max_frame",
          "[binding_data][time_offset]") {
    // current_frame=98, offset=+5 → raw=103 → clamped to 99
    CHECK(computeEncodingFrame(98, 0, 5, 99) == 99);
    // current_frame=99, offset=+1 → raw=100 → clamped to 99
    CHECK(computeEncodingFrame(99, 0, 1, 99) == 99);
}

TEST_CASE("computeEncodingFrame - batch + offset clamps to max_frame",
          "[binding_data][time_offset]") {
    // current_frame=95, batch=3, offset=5 → raw=103 → clamped to 99
    CHECK(computeEncodingFrame(95, 3, 5, 99) == 99);
}

TEST_CASE("computeEncodingFrame - batch + negative offset clamps to zero",
          "[binding_data][time_offset]") {
    // current_frame=1, batch=0, offset=-5 → raw=-4 → clamped to 0
    CHECK(computeEncodingFrame(1, 0, -5, 50) == 0);
}

TEST_CASE("computeEncodingFrame - exact boundaries are not clamped",
          "[binding_data][time_offset]") {
    CHECK(computeEncodingFrame(0, 0, 0, 99) == 0);
    CHECK(computeEncodingFrame(99, 0, 0, 99) == 99);
}

// ════════════════════════════════════════════════════════════════════════════
// SlotBindingData defaults
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("SlotBindingData - time_offset defaults to zero",
          "[binding_data][time_offset]") {
    SlotBindingData const binding;
    CHECK(binding.time_offset == 0);
}
