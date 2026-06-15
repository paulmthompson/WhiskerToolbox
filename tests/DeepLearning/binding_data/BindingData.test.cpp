/**
 * @file BindingData.test.cpp
 * @brief Tests for DeepLearningBindingData utilities.
 *
 * Exercises the computeEncodingFrame() helper that combines current frame,
 * batch index, and time_offset with clamping to [0, max_frame].
 */

#include <catch2/catch_test_macros.hpp>

// Pull in the header under test — no torch or Qt dependency needed.
#include "DeepLearning/bindings/DeepLearningBindingData.hpp"

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
// StaticInputSource
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("StaticInputSource - staticInputSourceToString round trips",
          "[binding_data][static_source]") {
    CHECK(staticInputSourceToString(StaticInputSource::DataManager) ==
          "DataManager");
    CHECK(staticInputSourceToString(StaticInputSource::DataBank) == "DataBank");
}

TEST_CASE("StaticInputSource - staticInputSourceFromString round trips",
          "[binding_data][static_source]") {
    CHECK(staticInputSourceFromString("DataManager") ==
          StaticInputSource::DataManager);
    CHECK(staticInputSourceFromString("DataBank") == StaticInputSource::DataBank);
}

TEST_CASE("StaticInputSource - unknown string defaults to DataManager",
          "[binding_data][static_source]") {
    CHECK(staticInputSourceFromString("") == StaticInputSource::DataManager);
    CHECK(staticInputSourceFromString("unknown") ==
          StaticInputSource::DataManager);
}

// ════════════════════════════════════════════════════════════════════════════
// MemoryFrameBinding
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("MemoryFrameBinding - static defaults",
          "[binding_data][memory_frame]") {
    dl::MemoryFrameBinding const frame;
    CHECK(frame.slot_name.empty());
    CHECK(frame.memory_index == 0);
    CHECK(dl::isStaticFrame(frame));
    CHECK_FALSE(dl::hasActiveStaticSource(frame));
}

TEST_CASE("MemoryFrameBinding - static DataManager source",
          "[binding_data][memory_frame]") {
    dl::MemoryFrameBinding frame;
    frame.slot_name = "memory_masks";
    frame.frame = dl::StaticFrameSource{
            dl::DataManagerStaticSource{.data_key = "masks/ref", .time_offset = 2}};
    CHECK(dl::staticSourceType(frame) == StaticInputSource::DataManager);
    CHECK(dl::staticDataKey(frame) == "masks/ref");
    CHECK(dl::staticTimeOffset(frame) == 2);
    CHECK(dl::hasActiveStaticSource(frame));
}

TEST_CASE("MemoryFrameBinding - static DataBank source",
          "[binding_data][memory_frame]") {
    dl::MemoryFrameBinding frame;
    frame.slot_name = "memory_masks";
    frame.memory_index = 1;
    frame.frame = dl::StaticFrameSource{
            dl::DataBankStaticSource{.bank_entry_id = "custom_ref"}};
    CHECK(dl::staticSourceType(frame) == StaticInputSource::DataBank);
    CHECK(dl::resolvedBankEntryId(frame) == "custom_ref");
    CHECK(dl::hasActiveStaticSource(frame));
}

TEST_CASE("MemoryFrameBinding - recurrent frame",
          "[binding_data][memory_frame]") {
    dl::MemoryFrameBinding frame;
    frame.slot_name = "memory_masks";
    frame.frame = dl::RecurrentFrameSource{
            .output_slot_name = "mask_out",
            .init = dl::StaticCaptureInit{.data_key = "media/video", .frame = 42}};
    CHECK(dl::isRecurrentFrame(frame));
    CHECK(dl::hasActiveRecurrentBinding(frame));
    CHECK(dl::recurrentInitMode(frame) == RecurrentInitMode::StaticCapture);
    CHECK(dl::staticCaptureDataKey(frame) == "media/video");
    CHECK(dl::staticCaptureFrame(frame) == 42);
}

TEST_CASE("hasActiveRecurrentBindings detects active frames",
          "[binding_data][memory_frame]") {
    dl::MemoryFrameBinding inactive;
    inactive.frame = dl::RecurrentFrameSource{};
    dl::MemoryFrameBinding active;
    active.frame = dl::RecurrentFrameSource{.output_slot_name = "out"};
  CHECK_FALSE(dl::hasActiveRecurrentBindings({inactive}));
  CHECK(dl::hasActiveRecurrentBindings({inactive, active}));
}

// ════════════════════════════════════════════════════════════════════════════
// RecurrentInitMode
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("RecurrentInitMode - recurrentInitModeToString round trips",
          "[binding_data][recurrent]") {
    CHECK(recurrentInitModeToString(RecurrentInitMode::Zeros) == "Zeros");
    CHECK(recurrentInitModeToString(RecurrentInitMode::StaticCapture) == "StaticCapture");
    CHECK(recurrentInitModeToString(RecurrentInitMode::FirstOutput) == "FirstOutput");
}

TEST_CASE("RecurrentInitMode - recurrentInitModeFromString round trips",
          "[binding_data][recurrent]") {
    CHECK(recurrentInitModeFromString("Zeros") == RecurrentInitMode::Zeros);
    CHECK(recurrentInitModeFromString("StaticCapture") == RecurrentInitMode::StaticCapture);
    CHECK(recurrentInitModeFromString("FirstOutput") == RecurrentInitMode::FirstOutput);
}

TEST_CASE("RecurrentInitMode - recurrentInitModeFromString defaults to Zeros on unknown",
          "[binding_data][recurrent]") {
    CHECK(recurrentInitModeFromString("") == RecurrentInitMode::Zeros);
    CHECK(recurrentInitModeFromString("unknown") == RecurrentInitMode::Zeros);
    CHECK(recurrentInitModeFromString("zeros") == RecurrentInitMode::Zeros);// case-sensitive
}

// ════════════════════════════════════════════════════════════════════════════
// recurrentCacheKey
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("recurrentCacheKey - generates correct format",
          "[binding_data][recurrent]") {
    CHECK(recurrentCacheKey("memory_output") == "recurrent:memory_output");
    CHECK(recurrentCacheKey("encoder_image") == "recurrent:encoder_image");
}

TEST_CASE("recurrentCacheKey - different slots produce different keys",
          "[binding_data][recurrent]") {
    CHECK(recurrentCacheKey("a") != recurrentCacheKey("b"));
}

