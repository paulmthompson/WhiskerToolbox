/// @file ConstraintEnforcer.test.cpp
/// @brief Tests for dl::constraints pure constraint computation functions.
///
/// Covers:
///   - computeBatchSizeConstraint: dynamic / fixed / recurrent-only batch modes,
///     active vs. inactive recurrent bindings, mixed combinations.
///   - validDecodersForModule: spatial-collapsing vs. spatial-preserving modules,
///     unknown / empty module type strings.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/Core/ConstraintEnforcer.hpp"
#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"// ModelDisplayInfo

#include "models_v2/TensorSlotDescriptor.hpp"// dl::DynamicBatch, FixedBatch, RecurrentOnlyBatch

using dl::constraints::BatchSizeConstraint;
using dl::constraints::computeBatchSizeConstraint;
using dl::constraints::validDecodersForModule;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Build a ModelDisplayInfo with only the batch_mode field set.
ModelDisplayInfo makeInfo(dl::BatchMode mode) {
    ModelDisplayInfo info;
    info.batch_mode = mode;
    return info;
}

/// Build an active RecurrentBindingData (output_slot_name is non-empty).
RecurrentBindingData makeActiveBinding(std::string const & output_slot = "encoder_output") {
    RecurrentBindingData rb;
    rb.input_slot_name = "memory";
    rb.output_slot_name = output_slot;
    return rb;
}

/// Build an inactive RecurrentBindingData (output_slot_name is empty).
RecurrentBindingData makeInactiveBinding() {
    RecurrentBindingData rb;
    rb.input_slot_name = "memory";
    rb.output_slot_name = "";
    return rb;
}

}// namespace

// ============================================================================
// computeBatchSizeConstraint — DynamicBatch mode
// ============================================================================

TEST_CASE("computeBatchSizeConstraint: DynamicBatch, no bindings",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::DynamicBatch{1, 0});
    BatchSizeConstraint const c = computeBatchSizeConstraint(info, {});

    CHECK(c.min == 1);
    CHECK(c.max == 0);// unlimited
    CHECK(!c.forced_by_recurrent);
}

TEST_CASE("computeBatchSizeConstraint: DynamicBatch with explicit range, no bindings",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::DynamicBatch{2, 8});
    BatchSizeConstraint const c = computeBatchSizeConstraint(info, {});

    CHECK(c.min == 2);
    CHECK(c.max == 8);
    CHECK(!c.forced_by_recurrent);
}

TEST_CASE("computeBatchSizeConstraint: DynamicBatch, active recurrent binding",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::DynamicBatch{1, 0});
    BatchSizeConstraint const c =
            computeBatchSizeConstraint(info, {makeActiveBinding()});

    CHECK(c.min == 1);
    CHECK(c.max == 1);
    CHECK(c.forced_by_recurrent);
}

TEST_CASE("computeBatchSizeConstraint: DynamicBatch, inactive bindings only",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::DynamicBatch{1, 16});
    BatchSizeConstraint const c =
            computeBatchSizeConstraint(info, {makeInactiveBinding(), makeInactiveBinding()});

    CHECK(c.min == 1);
    CHECK(c.max == 16);
    CHECK(!c.forced_by_recurrent);
}

TEST_CASE("computeBatchSizeConstraint: DynamicBatch, mixed active and inactive bindings",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::DynamicBatch{1, 0});
    BatchSizeConstraint const c =
            computeBatchSizeConstraint(info, {makeInactiveBinding(), makeActiveBinding()});

    // One active binding is enough to force batch=1
    CHECK(c.min == 1);
    CHECK(c.max == 1);
    CHECK(c.forced_by_recurrent);
}

// ============================================================================
// computeBatchSizeConstraint — FixedBatch mode
// ============================================================================

TEST_CASE("computeBatchSizeConstraint: FixedBatch{4}, no bindings",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::FixedBatch{4});
    BatchSizeConstraint const c = computeBatchSizeConstraint(info, {});

    CHECK(c.min == 4);
    CHECK(c.max == 4);
    CHECK(!c.forced_by_recurrent);
}

TEST_CASE("computeBatchSizeConstraint: FixedBatch{1}, no bindings — model-locked",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::FixedBatch{1});
    BatchSizeConstraint const c = computeBatchSizeConstraint(info, {});

    CHECK(c.min == 1);
    CHECK(c.max == 1);
    // Locked by model, not by recurrent bindings
    CHECK(!c.forced_by_recurrent);
}

TEST_CASE("computeBatchSizeConstraint: FixedBatch{4}, active recurrent binding — recurrent wins",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::FixedBatch{4});
    BatchSizeConstraint const c =
            computeBatchSizeConstraint(info, {makeActiveBinding()});

    // Recurrent constraint overrides FixedBatch{4} and locks to 1
    CHECK(c.min == 1);
    CHECK(c.max == 1);
    CHECK(c.forced_by_recurrent);
}

// ============================================================================
// computeBatchSizeConstraint — RecurrentOnlyBatch mode
// ============================================================================

TEST_CASE("computeBatchSizeConstraint: RecurrentOnlyBatch, no bindings",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::RecurrentOnlyBatch{});
    BatchSizeConstraint const c = computeBatchSizeConstraint(info, {});

    CHECK(c.min == 1);
    CHECK(c.max == 1);
    // Locked by model mode, not user recurrent binding
    CHECK(!c.forced_by_recurrent);
}

TEST_CASE("computeBatchSizeConstraint: RecurrentOnlyBatch, active binding — already locked",
          "[dl_constraints][batch_size]") {
    auto const info = makeInfo(dl::RecurrentOnlyBatch{});
    BatchSizeConstraint const c =
            computeBatchSizeConstraint(info, {makeActiveBinding()});

    CHECK(c.min == 1);
    CHECK(c.max == 1);
    // Active binding exists — forced_by_recurrent=true (user binding takes credit)
    CHECK(c.forced_by_recurrent);
}

// ============================================================================
// validDecodersForModule
// ============================================================================

TEST_CASE("validDecodersForModule: 'none' returns all decoders",
          "[dl_constraints][decoder_compat]") {
    auto const decoders = validDecodersForModule("none");

    REQUIRE(decoders.size() == 4);
    CHECK(std::find(decoders.begin(), decoders.end(), "MaskDecoderParams") != decoders.end());
    CHECK(std::find(decoders.begin(), decoders.end(), "PointDecoderParams") != decoders.end());
    CHECK(std::find(decoders.begin(), decoders.end(), "LineDecoderParams") != decoders.end());
    CHECK(std::find(decoders.begin(), decoders.end(), "FeatureVectorDecoderParams") !=
          decoders.end());
}

TEST_CASE("validDecodersForModule: 'global_avg_pool' restricts to feature-vector only",
          "[dl_constraints][decoder_compat]") {
    auto const decoders = validDecodersForModule("global_avg_pool");

    REQUIRE(decoders.size() == 1);
    CHECK(decoders[0] == "FeatureVectorDecoderParams");
}

TEST_CASE("validDecodersForModule: 'spatial_point' restricts to feature-vector only",
          "[dl_constraints][decoder_compat]") {
    auto const decoders = validDecodersForModule("spatial_point");

    REQUIRE(decoders.size() == 1);
    CHECK(decoders[0] == "FeatureVectorDecoderParams");
}

TEST_CASE("validDecodersForModule: empty string treated as no spatial collapse",
          "[dl_constraints][decoder_compat]") {
    auto const decoders = validDecodersForModule("");

    CHECK(decoders.size() == 4);
}

TEST_CASE("validDecodersForModule: unknown string treated as no spatial collapse",
          "[dl_constraints][decoder_compat]") {
    auto const decoders = validDecodersForModule("some_future_module");

    CHECK(decoders.size() == 4);
}
