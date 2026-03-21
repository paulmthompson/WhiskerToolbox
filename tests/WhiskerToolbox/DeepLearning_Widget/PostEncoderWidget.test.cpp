/// @file PostEncoderWidget.test.cpp
/// @brief Tests for the PostEncoderWidget.
///
/// Verifies construction, parameter get/set, data source refresh,
/// moduleTypeForState, and paramsFromState restore.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/PostEncoderWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <QApplication>
#include <QSignalSpy>

// Ensure a QApplication exists for QWidget-based tests
namespace {
struct QtAppGuard {
    QtAppGuard() {
        if (QApplication::instance() == nullptr) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            // Intentionally leaked to avoid destruction-order issues with Catch2
            new QApplication(argc, const_cast<char **>(argv));
        }
    }
};
QtAppGuard const s_guard;
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
    CHECK(widget->moduleTypeForState() == "none");
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

    auto result = widget.params();
    result.module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::widget::NoPostEncoderParams>);
    });
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

    auto result = widget.params();
    result.module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::GlobalAvgPoolModuleParams>);
    });
    CHECK(widget.moduleTypeForState() == "global_avg_pool");
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

    auto result = widget.params();
    result.module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::SpatialPointModuleParams>);
        if constexpr (std::is_same_v<T, dl::SpatialPointModuleParams>) {
            CHECK(mod.point_key == "points/query");
        }
    });
    CHECK(widget.moduleTypeForState() == "spatial_point");
}

// ============================================================================
// paramsFromState / state restore
// ============================================================================

TEST_CASE("paramsFromState restores none",
          "[dl_widget][post_encoder_widget]") {
    auto params = dl::widget::PostEncoderWidget::paramsFromState("none", "");
    params.module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::widget::NoPostEncoderParams>);
    });
}

TEST_CASE("paramsFromState restores global_avg_pool",
          "[dl_widget][post_encoder_widget]") {
    auto params =
            dl::widget::PostEncoderWidget::paramsFromState("global_avg_pool", "");
    params.module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::GlobalAvgPoolModuleParams>);
    });
}

TEST_CASE("paramsFromState restores spatial_point with point_key",
          "[dl_widget][post_encoder_widget]") {
    auto params = dl::widget::PostEncoderWidget::paramsFromState(
            "spatial_point", "points/my_key");
    params.module.visit([&](auto const & mod) {
        using T = std::decay_t<decltype(mod)>;
        CHECK(std::is_same_v<T, dl::SpatialPointModuleParams>);
        if constexpr (std::is_same_v<T, dl::SpatialPointModuleParams>) {
            CHECK(mod.point_key == "points/my_key");
        }
    });
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

    auto result = widget.params();
    result.module.visit([&](auto const & mod) {
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
