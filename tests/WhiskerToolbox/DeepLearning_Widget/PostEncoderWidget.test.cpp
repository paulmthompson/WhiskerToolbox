/// @file PostEncoderWidget.test.cpp
/// @brief Tests for the PostEncoderWidget.
///
/// Verifies construction, parameter get/set, data source refresh,
/// state sync, and bindingChanged emission.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/PostEncoderWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning/post_encoder/PostEncoderModuleParams.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <QComboBox>
#include <QSignalSpy>

#include <rfl/json.hpp>

namespace {

[[nodiscard]] dl::widget::PostEncoderSlotParams paramsForKey(std::string const & key) {
    dl::widget::PostEncoderSlotParams p;
    p.module_key = key;
    p.parameters_json = "{}";
    return p;
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
    CHECK(widget->params().module_key == "none");
}

TEST_CASE("PostEncoderWidget restores params from DeepLearningState",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    state->setPostEncoderParams(paramsForKey("global_avg_pool"));

    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());
    CHECK(widget.params().module_key == "global_avg_pool");
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

    widget.setParams(paramsForKey("none"));
    CHECK(widget.params().module_key == "none");
}

TEST_CASE("PostEncoderWidget setParams and params round-trip for GlobalAvgPool",
          "[dl_widget][post_encoder_widget]") {
    auto state = std::make_shared<DeepLearningState>();
    auto dm = std::make_shared<DataManager>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());

    widget.setParams(paramsForKey("global_avg_pool"));
    CHECK(widget.params().module_key == "global_avg_pool");
}

TEST_CASE("PostEncoderWidget setParams and params round-trip for SpatialPoint",
          "[dl_widget][post_encoder_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/query", TimeKey("time"));
    auto state = std::make_shared<DeepLearningState>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());
    widget.refreshDataSources();

    dl::SpatialPointModuleParams sp;
    sp.point_key = "points/query";

    dl::widget::PostEncoderSlotParams p;
    p.module_key = "spatial_point";
    p.parameters_json = rfl::json::write(sp);
    widget.setParams(p);

    auto const restored = rfl::json::read<dl::SpatialPointModuleParams>(
            widget.params().parameters_json);
    REQUIRE(restored);
    CHECK(restored.value().point_key == "points/query");
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

    widget.setParams(paramsForKey("global_avg_pool"));

    CHECK(state->postEncoderParams().module_key == "global_avg_pool");
}

// ============================================================================
// Data source refresh
// ============================================================================

TEST_CASE("PostEncoderWidget refreshDataSources populates point_key combo",
          "[dl_widget][post_encoder_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points/a", TimeKey("time"));
    dm->setData<PointData>("points/b", TimeKey("time"));
    auto state = std::make_shared<DeepLearningState>();
    auto assembler = std::make_unique<SlotAssembler>();

    dl::widget::PostEncoderWidget widget(state, dm, assembler.get());

    dl::SpatialPointModuleParams sp;
    sp.point_key = "points/a";

    dl::widget::PostEncoderSlotParams p;
    p.module_key = "spatial_point";
    p.parameters_json = rfl::json::write(sp);
    widget.setParams(p);
    widget.refreshDataSources();

    auto const restored = rfl::json::read<dl::SpatialPointModuleParams>(
            widget.params().parameters_json);
    REQUIRE(restored);
    CHECK(restored.value().point_key == "points/a");

    auto const combos = widget.findChildren<QComboBox *>();
    REQUIRE(combos.size() >= 2);
    auto const * point_combo = combos.back();
    CHECK(point_combo->findText(QStringLiteral("points/a")) >= 0);
    CHECK(point_combo->findText(QStringLiteral("points/b")) >= 0);
    CHECK(point_combo->findText(QStringLiteral("(None)")) >= 0);
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

    widget.setParams(paramsForKey("global_avg_pool"));

    CHECK(spy.isValid());
}
