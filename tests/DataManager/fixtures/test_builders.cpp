#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "builders/AnalogTimeSeriesBuilder.hpp"
#include "builders/DigitalTimeSeriesBuilder.hpp"
#include "builders/LineDataBuilder.hpp"
#include "builders/MaskDataBuilder.hpp"
#include "builders/TimeFrameBuilder.hpp"

TEST_CASE("TimeFrameBuilder basic construction", "[fixtures][builders][timeframe]") {
    SECTION("Explicit times") {
        auto tf = TimeFrameBuilder()
                          .withTimes({0, 10, 20, 30})
                          .build();

        REQUIRE(tf != nullptr);
    }

    SECTION("Range construction") {
        auto tf = TimeFrameBuilder()
                          .withRange(0, 100, 10)
                          .build();

        REQUIRE(tf != nullptr);
    }

    SECTION("Count construction") {
        auto tf = TimeFrameBuilder()
                          .withCount(5)
                          .build();

        REQUIRE(tf != nullptr);
    }
}

TEST_CASE("AnalogTimeSeriesBuilder basic construction", "[fixtures][builders][analog]") {
    SECTION("Explicit values and times") {
        auto signal = AnalogTimeSeriesBuilder()
                              .withValues({1.0f, 2.0f, 3.0f})
                              .atTimes({0, 10, 20})
                              .build();

        REQUIRE(signal != nullptr);

        auto values = signal->getAnalogTimeSeries();
        REQUIRE(values.size() == 3);
        REQUIRE(values[0] == 1.0f);
        REQUIRE(values[1] == 2.0f);
        REQUIRE(values[2] == 3.0f);
    }

    SECTION("Triangle wave") {
        auto signal = AnalogTimeSeriesBuilder()
                              .withTriangleWave(0, 100, 50.0f)
                              .build();

        REQUIRE(signal != nullptr);

        auto values = signal->getAnalogTimeSeries();
        REQUIRE(values.size() == 101);
        REQUIRE(values[0] == 0.0f);
        REQUIRE(values[50] == Catch::Approx(50.0f));
        REQUIRE(values[100] == 0.0f);
    }

    SECTION("Sine wave") {
        auto signal = AnalogTimeSeriesBuilder()
                              .withSineWave(0, 100, 0.01f, 1.0f)
                              .build();

        REQUIRE(signal != nullptr);
        auto values = signal->getAnalogTimeSeries();
        REQUIRE(values.size() == 101);
    }

    SECTION("Constant value") {
        auto signal = AnalogTimeSeriesBuilder()
                              .withConstant(5.0f, 0, 10)
                              .build();

        REQUIRE(signal != nullptr);

        auto values = signal->getAnalogTimeSeries();
        REQUIRE(values.size() == 11);
        for (auto v: values) {
            REQUIRE(v == 5.0f);
        }
    }
}

TEST_CASE("MaskDataBuilder basic construction", "[fixtures][builders][mask]") {
    SECTION("Single mask at one time") {
        std::vector<uint32_t> xs = {1, 2, 3};
        std::vector<uint32_t> ys = {1, 2, 3};
        auto mask_data = MaskDataBuilder()
                                 .atTime(0, Mask2D(xs, ys))
                                 .build();

        REQUIRE(mask_data != nullptr);
        auto masks = mask_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(masks.size() == 1);
    }

    SECTION("Box mask using helper") {
        auto mask_data = MaskDataBuilder()
                                 .withBox(0, 10, 10, 5, 5)
                                 .build();

        REQUIRE(mask_data != nullptr);
        auto masks = mask_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].size() == 25);// 5x5 = 25 pixels
    }

    SECTION("Circle mask using helper") {
        auto mask_data = MaskDataBuilder()
                                 .withCircle(0, 50, 50, 5)
                                 .build();

        REQUIRE(mask_data != nullptr);
        auto masks = mask_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(masks.size() == 1);
        // Circle with radius 5 should have approximately 79 pixels (pi * r^2 ≈ 78.5)
        REQUIRE(masks[0].size() > 70);
        REQUIRE(masks[0].size() < 85);
    }

    SECTION("Multiple masks at different times") {
        std::vector<uint32_t> xs1 = {1, 2};
        std::vector<uint32_t> ys1 = {1, 2};
        std::vector<uint32_t> xs2 = {3, 4, 5};
        std::vector<uint32_t> ys2 = {3, 4, 5};
        auto mask_data = MaskDataBuilder()
                                 .atTime(0, Mask2D(xs1, ys1))
                                 .atTime(10, Mask2D(xs2, ys2))
                                 .build();

        REQUIRE(mask_data != nullptr);
        auto masks_t0 = mask_data->getAtTime(TimeFrameIndex(0));
        auto masks_t10 = mask_data->getAtTime(TimeFrameIndex(10));
        REQUIRE(masks_t0.size() == 1);
        REQUIRE(masks_t10.size() == 1);
    }
}

TEST_CASE("LineDataBuilder basic construction", "[fixtures][builders][line]") {
    SECTION("Horizontal line") {
        auto line_data = LineDataBuilder()
                                 .withHorizontal(0, 0.0f, 10.0f, 5.0f, 4)
                                 .build();

        REQUIRE(line_data != nullptr);
        auto lines = line_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 4);
    }

    SECTION("Vertical line") {
        auto line_data = LineDataBuilder()
                                 .withVertical(0, 5.0f, 0.0f, 10.0f, 4)
                                 .build();

        REQUIRE(line_data != nullptr);
        auto lines = line_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 4);
    }

    SECTION("Line from coordinates") {
        auto line_data = LineDataBuilder()
                                 .withCoords(0, {1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f})
                                 .build();

        REQUIRE(line_data != nullptr);
        auto lines = line_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 3);
    }
}

TEST_CASE("DigitalTimeSeriesBuilder basic construction", "[fixtures][builders][digital]") {
    SECTION("Event series with explicit times") {
        auto events = DigitalEventSeriesBuilder()
                              .withEvents({0, 10, 20, 30})
                              .build();

        REQUIRE(events != nullptr);
        REQUIRE(events->size() == 4);
    }

    SECTION("Event series with interval") {
        auto events = DigitalEventSeriesBuilder()
                              .withInterval(0, 100, 10)
                              .build();

        REQUIRE(events != nullptr);
        REQUIRE(events->size() == 11);// 0, 10, 20, ..., 100
    }

    SECTION("Interval series") {
        auto intervals = DigitalIntervalSeriesBuilder()
                                 .withInterval(0, 10)
                                 .withInterval(20, 30)
                                 .build();

        REQUIRE(intervals != nullptr);
        // Just check that we can get intervals in range
        auto interval_view = intervals->getIntervalsInRange(TimeFrameIndex(0),
                                                            TimeFrameIndex(100),
                                                            *(intervals->getTimeFrame()));
        std::vector<Interval> interval_data(interval_view.begin(), interval_view.end());
        REQUIRE(interval_data.size() >= 2);
    }

    SECTION("Interval series with pattern") {
        auto intervals = DigitalIntervalSeriesBuilder()
                                 .withPattern(0, 100, 10, 5)// 10-unit intervals with 5-unit gaps
                                 .build();

        REQUIRE(intervals != nullptr);
        auto interval_view = intervals->getIntervalsInRange(TimeFrameIndex(0),
                                                            TimeFrameIndex(100),
                                                            *(intervals->getTimeFrame()));
        std::vector<Interval> interval_data(interval_view.begin(), interval_view.end());
        REQUIRE(interval_data.size() > 0);
    }
}

TEST_CASE("Builder chaining and composition", "[fixtures][builders][composition]") {
    SECTION("Complex mask data with multiple shapes") {
        auto mask_data = MaskDataBuilder()
                                 .withBox(0, 10, 10, 5, 5)
                                 .withCircle(10, 50, 50, 3)
                                 .withPoint(20, 100, 100)
                                 .withImageSize(800, 600)
                                 .build();

        REQUIRE(mask_data != nullptr);
        // Validate each timestamp has data
        REQUIRE(mask_data->getAtTime(TimeFrameIndex(0)).size() > 0);
        REQUIRE(mask_data->getAtTime(TimeFrameIndex(10)).size() > 0);
        REQUIRE(mask_data->getAtTime(TimeFrameIndex(20)).size() > 0);
    }

    SECTION("Complex line data with multiple types") {
        auto line_data = LineDataBuilder()
                                 .withHorizontal(0, 0.0f, 10.0f, 5.0f)
                                 .withVertical(10, 5.0f, 0.0f, 10.0f)
                                 .withDiagonal(20, 0.0f, 0.0f, 10.0f)
                                 .withImageSize(800, 600)
                                 .build();

        REQUIRE(line_data != nullptr);
        // Validate each timestamp has data
        REQUIRE(line_data->getAtTime(TimeFrameIndex(0)).size() > 0);
        REQUIRE(line_data->getAtTime(TimeFrameIndex(10)).size() > 0);
        REQUIRE(line_data->getAtTime(TimeFrameIndex(20)).size() > 0);
    }
}
