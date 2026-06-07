/**
 * @file DataManagerMerge.test.cpp
 * @brief Unit tests for mergeOverwriteData() utility.
 */

#include <catch2/catch_test_macros.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "utils/DataManagerMerge.hpp"

#include <memory>
#include <string>

namespace {

[[nodiscard]] std::unique_ptr<DataManager> makeDataManagerWithTimeFrame() {
    auto dm = std::make_unique<DataManager>();
    auto const time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
    dm->setTime(TimeKey("test_time"), time_frame);
    return dm;
}

}// namespace

TEST_CASE("supportsMergeOverwrite - ragged types only", "[DataManagerMerge]") {
    REQUIRE(supportsMergeOverwrite(DataTypeVariant{std::make_shared<LineData>()}));
    REQUIRE(supportsMergeOverwrite(DataTypeVariant{std::make_shared<MaskData>()}));
    REQUIRE(supportsMergeOverwrite(DataTypeVariant{std::make_shared<PointData>()}));
    REQUIRE_FALSE(supportsMergeOverwrite(DataTypeVariant{std::make_shared<AnalogTimeSeries>(
            std::vector<float>{1.0f}, std::vector<TimeFrameIndex>{TimeFrameIndex(0)})}));
}

TEST_CASE("mergeOverwriteData - success for LineData", "[DataManagerMerge][Line]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<LineData>("target", TimeKey("test_time"));
    dm->setData<LineData>("source", TimeKey("test_time"));

    auto target = dm->getData<LineData>("target");
    auto source = dm->getData<LineData>("source");

    target->addAtTime(TimeFrameIndex(10),
                      Line2D(std::vector<float>{1.0f}, std::vector<float>{1.0f}),
                      NotifyObservers::No);
    source->addAtTime(TimeFrameIndex(10),
                      Line2D(std::vector<float>{2.0f}, std::vector<float>{2.0f}),
                      NotifyObservers::No);
    source->addAtTime(TimeFrameIndex(30),
                      Line2D(std::vector<float>{3.0f}, std::vector<float>{3.0f}),
                      NotifyObservers::No);

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "target", "source", error);

    REQUIRE(merged.has_value());
    REQUIRE(*merged == 2);
    REQUIRE(error.empty());
    REQUIRE(target->getAtTime(TimeFrameIndex(10)).size() == 1);
    REQUIRE(target->getAtTime(TimeFrameIndex(30)).size() == 1);
    REQUIRE(source->getAtTime(TimeFrameIndex(10)).size() == 1);
}

TEST_CASE("mergeOverwriteData - type mismatch", "[DataManagerMerge]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<LineData>("target", TimeKey("test_time"));
    dm->setData<MaskData>("source", TimeKey("test_time"));

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "target", "source", error);

    REQUIRE_FALSE(merged.has_value());
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("mergeOverwriteData - missing key", "[DataManagerMerge]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<LineData>("target", TimeKey("test_time"));

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "target", "missing", error);

    REQUIRE_FALSE(merged.has_value());
    REQUIRE(error.find("missing") != std::string::npos);
}

TEST_CASE("mergeOverwriteData - unsupported AnalogTimeSeries", "[DataManagerMerge][Analog]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<AnalogTimeSeries>(
            "target",
            std::make_shared<AnalogTimeSeries>(
                    std::vector<float>{1.0f},
                    std::vector<TimeFrameIndex>{TimeFrameIndex(0)}),
            TimeKey("test_time"));
    dm->setData<AnalogTimeSeries>(
            "source",
            std::make_shared<AnalogTimeSeries>(
                    std::vector<float>{2.0f},
                    std::vector<TimeFrameIndex>{TimeFrameIndex(10)}),
            TimeKey("test_time"));

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "target", "source", error);

    REQUIRE_FALSE(merged.has_value());
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("mergeOverwriteData - TimeFrame mismatch", "[DataManagerMerge][TimeFrame]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<LineData>("target", TimeKey("test_time"));
    dm->setData<LineData>("source", TimeKey("test_time"));

    auto target = dm->getData<LineData>("target");
    auto source = dm->getData<LineData>("source");

    target->setTimeFrame(std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30}));

    target->addAtTime(TimeFrameIndex(10),
                      Line2D(std::vector<float>{1.0f}, std::vector<float>{1.0f}),
                      NotifyObservers::No);
    source->addAtTime(TimeFrameIndex(10),
                      Line2D(std::vector<float>{2.0f}, std::vector<float>{2.0f}),
                      NotifyObservers::No);

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "target", "source", error);

    REQUIRE_FALSE(merged.has_value());
    REQUIRE(error.find("TimeFrame") != std::string::npos);
    REQUIRE(target->getAtTime(TimeFrameIndex(10)).size() == 1);
}

TEST_CASE("mergeOverwriteData - self-merge rejected", "[DataManagerMerge]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<LineData>("data", TimeKey("test_time"));

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "data", "data", error);

    REQUIRE_FALSE(merged.has_value());
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("mergeOverwriteData - variant source into existing target", "[DataManagerMerge][Variant]") {
    auto dm = makeDataManagerWithTimeFrame();
    auto const time_frame = dm->getTime(TimeKey("test_time"));

    dm->setData<LineData>("output", TimeKey("test_time"));
    auto target = dm->getData<LineData>("output");
    target->addAtTime(TimeFrameIndex(10),
                      Line2D(std::vector<float>{1.0f}, std::vector<float>{1.0f}),
                      NotifyObservers::No);
    target->addAtTime(TimeFrameIndex(20),
                      Line2D(std::vector<float>{9.0f}, std::vector<float>{9.0f}),
                      NotifyObservers::No);

    auto pipeline_output = std::make_shared<LineData>();
    pipeline_output->setTimeFrame(time_frame);
    pipeline_output->addAtTime(TimeFrameIndex(10),
                               Line2D(std::vector<float>{2.0f}, std::vector<float>{2.0f}),
                               NotifyObservers::No);
    pipeline_output->addAtTime(TimeFrameIndex(30),
                               Line2D(std::vector<float>{3.0f}, std::vector<float>{3.0f}),
                               NotifyObservers::No);

    DataTypeVariant const source_variant{pipeline_output};

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "output", source_variant, error);

    REQUIRE(merged.has_value());
    REQUIRE(*merged == 2);
    REQUIRE(error.empty());
    REQUIRE(target->getAtTime(TimeFrameIndex(10)).size() == 1);
    REQUIRE(target->getAtTime(TimeFrameIndex(20)).size() == 1);
    REQUIRE(target->getAtTime(TimeFrameIndex(30)).size() == 1);
    REQUIRE(pipeline_output->getAtTime(TimeFrameIndex(10)).size() == 1);
}

TEST_CASE("mergeOverwriteData - variant source type mismatch", "[DataManagerMerge][Variant]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<LineData>("output", TimeKey("test_time"));

    auto pipeline_output = std::make_shared<MaskData>();
    pipeline_output->setTimeFrame(dm->getTime(TimeKey("test_time")));
    DataTypeVariant const source_variant{pipeline_output};

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "output", source_variant, error);

    REQUIRE_FALSE(merged.has_value());
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("mergeOverwriteData - variant source TimeFrame mismatch", "[DataManagerMerge][Variant]") {
    auto dm = makeDataManagerWithTimeFrame();
    dm->setData<LineData>("output", TimeKey("test_time"));

    auto pipeline_output = std::make_shared<LineData>();
    pipeline_output->setTimeFrame(
            std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30}));
    pipeline_output->addAtTime(TimeFrameIndex(10),
                               Line2D(std::vector<float>{2.0f}, std::vector<float>{2.0f}),
                               NotifyObservers::No);

    DataTypeVariant const source_variant{pipeline_output};

    std::string error;
    auto const merged = mergeOverwriteData(*dm, "output", source_variant, error);

    REQUIRE_FALSE(merged.has_value());
    REQUIRE(error.find("TimeFrame") != std::string::npos);
}
