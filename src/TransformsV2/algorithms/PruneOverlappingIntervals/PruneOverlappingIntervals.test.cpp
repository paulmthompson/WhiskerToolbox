#include "PruneOverlappingIntervals.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;

namespace {

std::shared_ptr<DigitalIntervalSeries> makeIntervalSeries(std::vector<TimeFrameInterval> const & intervals) {
    return std::make_shared<DigitalIntervalSeries>(intervals);
}

std::shared_ptr<DigitalIntervalSeries> makeOverlappingIntervalSeries(std::vector<TimeFrameInterval> const & intervals) {
    return DigitalIntervalSeries::createOverlapping(intervals);
}

void requireIntervalsEqual(std::shared_ptr<DigitalIntervalSeries> const & series,
                           std::vector<TimeFrameInterval> const & expected) {
    REQUIRE(series != nullptr);
    REQUIRE(series->size() == expected.size());
    size_t idx = 0;
    for (auto const & interval_with_id: series->view()) {
        REQUIRE(interval_with_id.interval.start == expected[idx].start);
        REQUIRE(interval_with_id.interval.end == expected[idx].end);
        ++idx;
    }
}

}// namespace

TEST_CASE("V2 Container Transform: Prune Overlapping Intervals - Algorithm",
          "[transforms][v2][container][prune_overlapping_intervals]") {
    ComputeContext const ctx;
    PruneOverlappingIntervalsParams const params;

    SECTION("Empty input returns empty disjoint series") {
        DigitalIntervalSeries const empty;
        auto const result = pruneOverlappingIntervals(empty, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 0);
        REQUIRE(result->layout() == IntervalLayout::Disjoint);
    }

    SECTION("Single interval is kept") {
        auto intervals = makeIntervalSeries({
            {TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(200)}}
        });
        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(100), TimeFrameIndex(200)}}});
        REQUIRE(result->layout() == IntervalLayout::Disjoint);
    }

    SECTION("Well-separated intervals are all kept") {
        auto intervals = makeIntervalSeries({
            {TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(100)},
             TimeFrameInterval{TimeFrameIndex(200), TimeFrameIndex(250)},
             TimeFrameInterval{TimeFrameIndex(400), TimeFrameIndex(450)}}
        });
        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(100)},
             TimeFrameInterval{TimeFrameIndex(200), TimeFrameIndex(250)},
             TimeFrameInterval{TimeFrameIndex(400), TimeFrameIndex(450)}}});
    }

    SECTION("Overlapping intervals are pruned") {
        auto intervals = makeIntervalSeries({{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(70), TimeFrameIndex(170)},
             TimeFrameInterval{TimeFrameIndex(250), TimeFrameIndex(350)}}});
        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(250), TimeFrameIndex(350)}}});
    }

    SECTION("All overlapping except first are pruned") {
        auto intervals = makeIntervalSeries({{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(60), TimeFrameIndex(160)},
             TimeFrameInterval{TimeFrameIndex(70), TimeFrameIndex(170)}}});
        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)}}});
    }

    SECTION("Chain pruning keeps non-overlapping survivors") {
        auto intervals = makeIntervalSeries({{TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(50)},
             TimeFrameInterval{TimeFrameIndex(30), TimeFrameIndex(130)},
             TimeFrameInterval{TimeFrameIndex(110), TimeFrameIndex(210)},
             TimeFrameInterval{TimeFrameIndex(190), TimeFrameIndex(290)}}});
        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(50)},
             TimeFrameInterval{TimeFrameIndex(110), TimeFrameIndex(210)}}});
    }

    SECTION("Exactly touching intervals are pruned") {
        auto intervals = makeIntervalSeries({{TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(50)},
             TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)}}});
        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(0), TimeFrameIndex(50)}}});
    }

    SECTION("TimeFrame is preserved from input") {
        std::vector<int> const times{0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
        auto time_frame = std::make_shared<TimeFrame>(times);

        auto intervals = makeIntervalSeries({{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(70), TimeFrameIndex(170)},
             TimeFrameInterval{TimeFrameIndex(250), TimeFrameIndex(350)}}});
        intervals->setTimeFrame(time_frame);

        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->getTimeFrame() == time_frame);
    }

    SECTION("Overlapping input layout produces disjoint output") {
        auto intervals = makeOverlappingIntervalSeries({{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(70), TimeFrameIndex(170)},
             TimeFrameInterval{TimeFrameIndex(250), TimeFrameIndex(350)}}});
        REQUIRE(intervals->layout() == IntervalLayout::Overlapping);

        auto const result = pruneOverlappingIntervals(*intervals, params, ctx);

        requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(250), TimeFrameIndex(350)}}});
        REQUIRE(result->layout() == IntervalLayout::Disjoint);
    }
}

TEST_CASE("V2 Container Transform: Prune Overlapping Intervals - Registry",
          "[transforms][v2][container][prune_overlapping_intervals]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    REQUIRE(registry.hasTransform("PruneOverlappingIntervals"));

    auto intervals = makeOverlappingIntervalSeries({{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(70), TimeFrameIndex(170)},
             TimeFrameInterval{TimeFrameIndex(250), TimeFrameIndex(350)}}});

    auto const result = registry.executeContainerTransform<
            DigitalIntervalSeries,
            DigitalIntervalSeries,
            PruneOverlappingIntervalsParams>(
            "PruneOverlappingIntervals",
            *intervals,
            PruneOverlappingIntervalsParams{},
            ctx);

    requireIntervalsEqual(result, {{TimeFrameInterval{TimeFrameIndex(50), TimeFrameIndex(150)},
             TimeFrameInterval{TimeFrameIndex(250), TimeFrameIndex(350)}}});
    REQUIRE(result->layout() == IntervalLayout::Disjoint);
}
