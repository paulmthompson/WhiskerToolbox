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
// StaticInputData defaults
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("StaticInputData - defaults",
          "[binding_data][static_source]") {
    StaticInputData const si;
    CHECK(si.source_type_str == "DataManager");
    CHECK(si.sourceType() == StaticInputSource::DataManager);
    CHECK(si.time_offset == 0);
    CHECK(si.memory_index == 0);
    CHECK(si.active == true);
    CHECK(si.bank_entry_id.empty());
    CHECK_FALSE(si.hasActiveSource());
}

TEST_CASE("StaticInputData - setSourceType updates string",
          "[binding_data][static_source]") {
    StaticInputData si;
    si.setSourceType(StaticInputSource::DataBank);
    CHECK(si.source_type_str == "DataBank");
    CHECK(si.sourceType() == StaticInputSource::DataBank);

    si.setSourceType(StaticInputSource::DataManager);
    CHECK(si.source_type_str == "DataManager");
    CHECK(si.sourceType() == StaticInputSource::DataManager);
}

TEST_CASE("StaticInputData - resolvedBankEntryId prefers explicit bank_entry_id",
          "[binding_data][static_source]") {
    StaticInputData si;
    si.slot_name = "memory_masks";
    si.memory_index = 2;
    si.setSourceType(StaticInputSource::DataBank);
    si.bank_entry_id = "custom_ref";
    CHECK(si.resolvedBankEntryId() == "custom_ref");
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

// ════════════════════════════════════════════════════════════════════════════
// RecurrentBindingData defaults
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("RecurrentBindingData - defaults",
          "[binding_data][recurrent]") {
    RecurrentBindingData const rb;
    CHECK(rb.input_slot_name.empty());
    CHECK(rb.output_slot_name.empty());
    CHECK(rb.init_mode_str == "Zeros");
    CHECK(rb.init_data_key.empty());
    CHECK(rb.init_frame == -1);
    CHECK(rb.initMode() == RecurrentInitMode::Zeros);
}

TEST_CASE("RecurrentBindingData - setInitMode updates string",
          "[binding_data][recurrent]") {
    RecurrentBindingData rb;
    rb.setInitMode(RecurrentInitMode::StaticCapture);
    CHECK(rb.init_mode_str == "StaticCapture");
    CHECK(rb.initMode() == RecurrentInitMode::StaticCapture);

    rb.setInitMode(RecurrentInitMode::FirstOutput);
    CHECK(rb.init_mode_str == "FirstOutput");
    CHECK(rb.initMode() == RecurrentInitMode::FirstOutput);

    rb.setInitMode(RecurrentInitMode::Zeros);
    CHECK(rb.init_mode_str == "Zeros");
    CHECK(rb.initMode() == RecurrentInitMode::Zeros);
}

// ════════════════════════════════════════════════════════════════════════════
// StaticInputData for sequence entries
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("StaticInputData - multiple entries per slot with different memory_index",
          "[binding_data][sequence]") {
    std::vector<StaticInputData> entries;
    for (int i = 0; i < 4; ++i) {
        StaticInputData si;
        si.slot_name = "memory_images";
        si.memory_index = i;
        si.data_key = "media/video_1";
        si.setSourceType(StaticInputSource::DataBank);
        si.bank_entry_id = defaultBankEntryId("memory_images", i);
        entries.push_back(std::move(si));
    }

    CHECK(entries.size() == 4);
    for (int i = 0; i < 4; ++i) {
        CHECK(entries[static_cast<std::size_t>(i)].memory_index == i);
        CHECK(entries[static_cast<std::size_t>(i)].hasActiveSource());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Phase 5: Hybrid Sequence Inputs — RecurrentBindingData.target_memory_index
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("RecurrentBindingData - target_memory_index defaults to -1",
          "[binding_data][hybrid]") {
    RecurrentBindingData const rb;
    CHECK(rb.target_memory_index == -1);
    CHECK_FALSE(rb.hasTargetMemoryIndex());
}

TEST_CASE("RecurrentBindingData - hasTargetMemoryIndex with valid index",
          "[binding_data][hybrid]") {
    RecurrentBindingData rb;
    rb.target_memory_index = 0;
    CHECK(rb.hasTargetMemoryIndex());

    rb.target_memory_index = 3;
    CHECK(rb.hasTargetMemoryIndex());
}

TEST_CASE("RecurrentBindingData - hasTargetMemoryIndex with negative index",
          "[binding_data][hybrid]") {
    RecurrentBindingData rb;
    rb.target_memory_index = -1;
    CHECK_FALSE(rb.hasTargetMemoryIndex());

    rb.target_memory_index = -5;
    CHECK_FALSE(rb.hasTargetMemoryIndex());
}

TEST_CASE("RecurrentBindingData - hybrid binding has all fields",
          "[binding_data][hybrid]") {
    RecurrentBindingData rb;
    rb.input_slot_name = "memory_images";
    rb.output_slot_name = "decoder_output";
    rb.target_memory_index = 4;
    rb.setInitMode(RecurrentInitMode::Zeros);

    CHECK(rb.input_slot_name == "memory_images");
    CHECK(rb.output_slot_name == "decoder_output");
    CHECK(rb.target_memory_index == 4);
    CHECK(rb.hasTargetMemoryIndex());
    CHECK(rb.initMode() == RecurrentInitMode::Zeros);
}

TEST_CASE("RecurrentBindingData - whole-slot vs position-specific",
          "[binding_data][hybrid]") {
    RecurrentBindingData whole_slot;
    whole_slot.input_slot_name = "memory_images";
    whole_slot.output_slot_name = "output";
    // target_memory_index defaults to -1 (whole slot)

    RecurrentBindingData position_specific;
    position_specific.input_slot_name = "memory_images";
    position_specific.output_slot_name = "output";
    position_specific.target_memory_index = 2;

    CHECK_FALSE(whole_slot.hasTargetMemoryIndex());
    CHECK(position_specific.hasTargetMemoryIndex());
}

TEST_CASE("RecurrentBindingData - hybrid with StaticCapture init",
          "[binding_data][hybrid]") {
    RecurrentBindingData rb;
    rb.input_slot_name = "memory_images";
    rb.output_slot_name = "decoder_output";
    rb.target_memory_index = 4;
    rb.setInitMode(RecurrentInitMode::StaticCapture);
    rb.init_data_key = "media/video_1";
    rb.init_frame = 42;

    CHECK(rb.initMode() == RecurrentInitMode::StaticCapture);
    CHECK(rb.init_data_key == "media/video_1");
    CHECK(rb.init_frame == 42);
    CHECK(rb.hasTargetMemoryIndex());
}
