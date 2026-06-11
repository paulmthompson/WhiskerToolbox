/// @file PostEncoderWidget.test.cpp
/// @brief Tests for the PostEncoderWidget.
///
/// Verifies construction, parameter get/set, data source refresh,
/// state sync, and bindingChanged emission.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/PostEncoderWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <QSignalSpy>

namespace {

template<typename ExpectedModule>
void checkModuleIs(dl::widget::PostEncoderVariant const & module) {
    module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, ExpectedModule>);
    });
}

}// namespace

// ============================================================================
// Construction
// ============================================================================

TEST_CASE("PostEncoderWidget constructs with valid state, DM, and assembler",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    auto widget = std::make_unique<dl::widget::PostEncoderWidget>(
            state, dm, assembler.get());
    checkModuleIs<dl::widget::NoPostEncoderParams>(widget->params().module);
}

TEST_CASE("PostEncoderWidget restores params from DeepLearningState",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    dl::widget::PostEncoderSlotParams saved;
    saved.module = dl::GlobalAvgPoolModuleParams{};
    state->setPostEncoderParams(saved);

    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());
    checkModuleIs<dl::GlobalAvgPoolModuleParams>(widget.params().module);
}

// ============================================================================
// setParams / params round-trip
// ============================================================================

TEST_CASE("PostEncoderWidget setParams and params round-trip for None",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());

    dl::widget::PostEncoderSlotParams p;
    p.module = dl::widget::NoPostEncoderParams{};
    widget.setParams(p);

    checkModuleIs<dl::widget::NoPostEncoderParams>(widget.params().module);
}

TEST_CASE("PostEncoderWidget setParams and params round-trip for GlobalAvgPool",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());

    dl::widget::PostEncoderSlotParams p;
    p.module = dl::GlobalAvgPoolModuleParams{};
    widget.setParams(p);

    checkModuleIs<dl::GlobalAvgPoolModuleParams>(widget.params().module);
}

TEST_CASE("PostEncoderWidget setParams and params round-trip for SpatialPoint",
          "[dl_widget][post_encoder_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/query", TimeKey("time"));
    auto state = std::make_shared<DeepLearningState>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());
    widget.refreshDataSources();

    dl::widget::PostEncoderSlotParams p;
    dl::SpatialPointModuleParams sp;
    sp.point_key = "points/query";
    p.module = sp;
    widget.setParams(p);

    widget.params().module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::SpatialPointModuleParams>);
        if constexpr (std::is_same_v<T, dl::SpatialPointModuleParams>) {
            CHECK(mod.point_key == "points/query");
        }
    });
}

// ============================================================================
// State sync
// ============================================================================

TEST_CASE("PostEncoderWidget syncs params to DeepLearningState on change",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());

    dl::widget::PostEncoderSlotParams p;
    p.module = dl::GlobalAvgPoolModuleParams{};
    widget.setParams(p);

    checkModuleIs<dl::GlobalAvgPoolModuleParams>(state->postEncoderParams().module);
}

// ============================================================================
// Data source refresh
// ============================================================================

TEST_CASE("PostEncoderWidget refreshDataSources updates point_key combo",
          "[dl_widget][post_encoder_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/a", TimeKey("time"));
    auto state = std::make_shared<DeepLearningState>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());

    dl::widget::PostEncoderSlotParams p;
    dl::SpatialPointModuleParams sp;
    sp.point_key = "points/a";
    p.module = sp;
    widget.setParams(p);
    widget.refreshDataSources();

    widget.params().module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        if constexpr (std::is_same_v<T, dl::SpatialPointModuleParams>) {
            CHECK(mod.point_key == "points/a");
        }
    });
}

// ============================================================================
// Signal emission
// ============================================================================

TEST_CASE("PostEncoderWidget emits bindingChanged on param changes",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());

    QSignalSpy const spy(&widget,
                         &dl::widget::PostEncoderWidget::bindingChanged);

    dl::widget::PostEncoderSlotParams p;
    p.module = dl::GlobalAvgPoolModuleParams{};
    widget.setParams(p);

    CHECK(spy.isValid());
}
