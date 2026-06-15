/// @file MemorySlotWidget.test.cpp
/// @brief Tests for MemorySlotWidget.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/MemorySlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"

#include <QApplication>
#include <QSignalSpy>

namespace {
struct QtAppGuard {
    QtAppGuard() {
        if (QApplication::instance() == nullptr) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            new QApplication(argc, const_cast<char **>(argv));
        }
    }
};
QtAppGuard const s_guard;
} // namespace

static dl::TensorSlotDescriptor makeMemorySlot(
        std::string name = "memory_masks",
        bool sequence = false) {
    dl::TensorSlotDescriptor slot;
    slot.name = std::move(name);
    if (sequence) {
        slot.shape = {4, 64, 64};
        slot.sequence_dim = 0;
    } else {
        slot.shape = {64, 64};
        slot.sequence_dim = -1;
    }
    slot.recommended_encoder = "MaskEncoder";
    slot.is_static = true;
    return slot;
}

TEST_CASE("MemorySlotWidget constructs for n=1 slot",
          "[dl_widget][memory_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    auto widget = std::make_unique<dl::widget::MemorySlotWidget>(
            makeMemorySlot(), dm, std::vector<std::string>{"mask_out"});
    CHECK(widget->slotName() == "memory_masks");
}

TEST_CASE("MemorySlotWidget bindings empty by default",
          "[dl_widget][memory_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dl::widget::MemorySlotWidget widget(
            makeMemorySlot(), dm, std::vector<std::string>{});
    CHECK(widget.bindings().empty());
}

TEST_CASE("MemorySlotWidget setBindings restores static frame",
          "[dl_widget][memory_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts/ref", TimeKey("time"));
    dl::widget::MemorySlotWidget widget(
            makeMemorySlot("memory_sequence", true), dm, {});

    dl::MemoryFrameBinding frame;
    frame.slot_name = "memory_sequence";
    frame.memory_index = 0;
    frame.frame = dl::StaticFrameSource{
            dl::DataManagerStaticSource{.data_key = "pts/ref", .time_offset = -1}};

    widget.setBindings({frame});

    auto result = widget.bindings();
    REQUIRE(result.size() == 1);
    CHECK(result[0].slot_name == "memory_sequence");
    CHECK(dl::staticDataKey(result[0]) == "pts/ref");
    CHECK(dl::staticTimeOffset(result[0]) == -1);
}

TEST_CASE("MemorySlotWidget setBindings restores recurrent frame",
          "[dl_widget][memory_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dl::widget::MemorySlotWidget widget(
            makeMemorySlot(), dm, std::vector<std::string>{"mask_out"});

    dl::MemoryFrameBinding frame;
    frame.slot_name = "memory_masks";
    frame.memory_index = 0;
    frame.frame = dl::RecurrentFrameSource{
            .output_slot_name = "mask_out",
            .init = dl::ZerosInit{}};

    widget.setBindings({frame});

    auto result = widget.bindings();
    REQUIRE(result.size() == 1);
    CHECK(dl::recurrentOutputSlot(result[0]) == "mask_out");
    CHECK(dl::recurrentInitMode(result[0]) == RecurrentInitMode::Zeros);
}

TEST_CASE("MemorySlotWidget bindingChanged signal is valid",
          "[dl_widget][memory_slot_widget]") {
    auto dm = std::make_shared<DataManager>();
    dl::widget::MemorySlotWidget widget(makeMemorySlot(), dm, {});
    QSignalSpy const spy(&widget, &dl::widget::MemorySlotWidget::bindingChanged);
    CHECK(spy.isValid());
}
