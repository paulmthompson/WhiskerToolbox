#include "digital_interval_invert.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "transforms/data_transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

TEST_CASE("Digital Interval Invert Transform", "[transforms][digital_interval_invert]") {
    InvertParams params;
    InvertIntervalOperation operation;
    DataTypeVariant variant;

    SECTION("Basic inversion with unbounded domain") {
        // Create test intervals: (5,10), (13,20), (23,40), (56,70), (72,91)
        std::vector<Interval> input_intervals = {
            {5, 10}, {13, 20}, {23, 40}, {56, 70}, {72, 91}
        };

        auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.domainType = DomainType::Unbounded;

        auto result = invert_intervals(input_series.get(), params);

        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();

        // Expected: (10,13), (20,23), (40,56), (70,72) -> exact boundary values
        REQUIRE(result_intervals.size() == 4);
        REQUIRE(result_intervals[0].start == 10);
        REQUIRE(result_intervals[0].end == 13);
        REQUIRE(result_intervals[1].start == 20);
        REQUIRE(result_intervals[1].end == 23);
        REQUIRE(result_intervals[2].start == 40);
        REQUIRE(result_intervals[2].end == 56);
        REQUIRE(result_intervals[3].start == 70);
        REQUIRE(result_intervals[3].end == 72);
    }

    SECTION("Bounded inversion") {
        // Create test intervals: (5,10), (13,20), (23,40), (56,70), (72,91)
        std::vector<Interval> input_intervals = {
            {5, 10}, {13, 20}, {23, 40}, {56, 70}, {72, 91}
        };

        auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.domainType = DomainType::Bounded;
        params.boundStart = 0.0;
        params.boundEnd = 100.0;

        auto result = invert_intervals(input_series.get(), params);

        REQUIRE(result != nullptr);

        auto const & result_intervals = result->getDigitalIntervalSeries();

        // Expected: (0,5), (10,13), (20,23), (40,56), (70,72), (91,100)
        // -> exact boundary values
        REQUIRE(result_intervals.size() == 6);
        REQUIRE(result_intervals[0].start == 0);
        REQUIRE(result_intervals[0].end == 5);
        REQUIRE(result_intervals[1].start == 10);
        REQUIRE(result_intervals[1].end == 13);
        REQUIRE(result_intervals[2].start == 20);
        REQUIRE(result_intervals[2].end == 23);
        REQUIRE(result_intervals[3].start == 40);
        REQUIRE(result_intervals[3].end == 56);
        REQUIRE(result_intervals[4].start == 70);
        REQUIRE(result_intervals[4].end == 72);
        REQUIRE(result_intervals[5].start == 91);
        REQUIRE(result_intervals[5].end == 100);
    }

    SECTION("Empty input with unbounded domain") {
        std::vector<Interval> input_intervals = {};
        auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.domainType = DomainType::Unbounded;

        auto result = invert_intervals(input_series.get(), params);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 0);
    }

    SECTION("Empty input with bounded domain") {
        std::vector<Interval> input_intervals = {};
        auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.domainType = DomainType::Bounded;
        params.boundStart = 0.0;
        params.boundEnd = 100.0;

        auto result = invert_intervals(input_series.get(), params);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->getDigitalIntervalSeries();

        // Should return the entire domain as one interval
        REQUIRE(result_intervals.size() == 1);
        REQUIRE(result_intervals[0].start == 0);
        REQUIRE(result_intervals[0].end == 100);
    }

    SECTION("Single interval") {
        std::vector<Interval> input_intervals = {{10, 20}};
        auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.domainType = DomainType::Bounded;
        params.boundStart = 0.0;
        params.boundEnd = 30.0;

        auto result = invert_intervals(input_series.get(), params);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->getDigitalIntervalSeries();

        // Expected: (0,10), (20,30) -> exact boundary values
        REQUIRE(result_intervals.size() == 2);
        REQUIRE(result_intervals[0].start == 0);
        REQUIRE(result_intervals[0].end == 10);
        REQUIRE(result_intervals[1].start == 20);
        REQUIRE(result_intervals[1].end == 30);
    }

    SECTION("Adjacent intervals - no gaps") {
        std::vector<Interval> input_intervals = {{5, 10}, {10, 20}};
        auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

        params.domainType = DomainType::Unbounded;

        auto result = invert_intervals(input_series.get(), params);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->getDigitalIntervalSeries();

        // No gaps between intervals since they share endpoint 10, so result should be empty
        REQUIRE(result_intervals.size() == 0);
    }

    SECTION("Operation interface validation") {
        // Test name
        REQUIRE(operation.getName() == "Invert Intervals");

        // Test target type index
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<DigitalIntervalSeries>));

        // Test default parameters
        auto default_params = operation.getDefaultParameters();
        REQUIRE(default_params != nullptr);

        auto* invert_params = dynamic_cast<InvertParams*>(default_params.get());
        REQUIRE(invert_params != nullptr);
        REQUIRE(invert_params->domainType == DomainType::Unbounded);
        REQUIRE(invert_params->boundStart == 0.0);
        REQUIRE(invert_params->boundEnd == 100.0);
    }

    SECTION("Null input handling") {
        params.domainType = DomainType::Unbounded;

        auto result = invert_intervals(nullptr, params);

        REQUIRE(result != nullptr);
        auto const & result_intervals = result->getDigitalIntervalSeries();
        REQUIRE(result_intervals.size() == 0);
    }
}
