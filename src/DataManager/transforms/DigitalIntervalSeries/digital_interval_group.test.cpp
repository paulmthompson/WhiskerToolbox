#include "digital_interval_group.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "transforms/data_transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

// Helper function to validate that grouped intervals don't violate spacing constraints
auto validateGrouping = [](std::vector<Interval> const & original,
                           std::vector<Interval> const & grouped,
                           double maxSpacing) -> bool {
    // Check that all grouped intervals respect the spacing constraint
    for (size_t i = 1; i < grouped.size(); ++i) {
        int64_t gap = grouped[i].start - grouped[i - 1].end - 1;
        if (gap <= static_cast<int64_t>(maxSpacing)) {
            return false;// Adjacent groups should be separated by more than maxSpacing
        }
    }

    // Check that all original intervals are covered by the grouped intervals
    for (auto const & orig: original) {
        bool covered = false;
        for (auto const & group: grouped) {
            if (orig.start >= group.start && orig.end <= group.end) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            return false;
        }
    }

    return true;
};

TEST_CASE("Digital Interval Group Transform", "[transforms][digital_interval_group]") {
    GroupParams params;
    GroupOperation operation;
    DataTypeVariant variant;

    SECTION("Basic grouping functionality") {
        // Test the example from documentation: (1,2), (4,5), (10,11) with spacing=3
        std::vector<Interval> intervals = {{1, 2}, {4, 5}, {10, 11}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 3.0;

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 2);

        // First group: (1,2) and (4,5) combined to (1,5)
        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 5);

        // Second group: (10,11) remains separate
        REQUIRE(grouped[1].start == 10);
        REQUIRE(grouped[1].end == 11);

        // Validate grouping constraints
        REQUIRE(validateGrouping(intervals, grouped, params.maxSpacing));
    }

    SECTION("No grouping needed - all intervals separate") {
        std::vector<Interval> intervals = {{1, 2}, {10, 11}, {20, 21}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 3.0;// Gaps are 7 and 8, both > 3

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 3);// No grouping should occur

        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 2);
        REQUIRE(grouped[1].start == 10);
        REQUIRE(grouped[1].end == 11);
        REQUIRE(grouped[2].start == 20);
        REQUIRE(grouped[2].end == 21);

        REQUIRE(validateGrouping(intervals, grouped, params.maxSpacing));
    }

    SECTION("All intervals grouped into one") {
        std::vector<Interval> intervals = {{1, 2}, {4, 5}, {7, 8}, {10, 11}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 2.0;// All gaps are ≤ 2

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 1);// All should be grouped

        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 11);

        REQUIRE(validateGrouping(intervals, grouped, params.maxSpacing));
    }

    SECTION("Zero spacing - only adjacent intervals group") {
        std::vector<Interval> intervals = {{1, 2}, {3, 4}, {6, 7}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 0.0;// Only touching intervals group

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 2);

        // (1,2) and (3,4) are adjacent (gap = 0), so they group
        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 4);

        // (6,7) is separate (gap = 1 > 0)
        REQUIRE(grouped[1].start == 6);
        REQUIRE(grouped[1].end == 7);

        REQUIRE(validateGrouping(intervals, grouped, params.maxSpacing));
    }

    SECTION("Large spacing - everything groups") {
        std::vector<Interval> intervals = {{1, 2}, {100, 101}, {200, 201}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 1000.0;// Very large spacing

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 1);

        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 201);

        REQUIRE(validateGrouping(intervals, grouped, params.maxSpacing));
    }

    SECTION("Overlapping intervals") {
        std::vector<Interval> intervals = {{1, 5}, {3, 7}, {10, 12}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 1.0;

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 2);

        // Overlapping intervals should be merged into one group
        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 7);

        REQUIRE(grouped[1].start == 10);
        REQUIRE(grouped[1].end == 12);
    }

    SECTION("Unsorted input intervals") {
        std::vector<Interval> intervals = {{10, 11}, {1, 2}, {4, 5}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 3.0;

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 2);

        // Should be sorted and grouped properly
        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 5);
        REQUIRE(grouped[1].start == 10);
        REQUIRE(grouped[1].end == 11);

        REQUIRE(validateGrouping(intervals, grouped, params.maxSpacing));
    }

    SECTION("Single interval") {
        std::vector<Interval> intervals = {{5, 10}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 1.0;

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 1);

        REQUIRE(grouped[0].start == 5);
        REQUIRE(grouped[0].end == 10);
    }

    SECTION("Empty input") {
        std::vector<Interval> intervals = {};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 1.0;

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.empty());
    }

    SECTION("Null input") {
        auto result = group_intervals(nullptr, params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }
}

TEST_CASE("Group Operation Class Tests", "[transforms][digital_interval_group][operation]") {
    GroupOperation operation;
    GroupParams params;
    DataTypeVariant variant;

    SECTION("Operation name and type info") {
        REQUIRE(operation.getName() == "Group Intervals");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<DigitalIntervalSeries>));
    }

    SECTION("canApply validation") {
        // Valid input
        std::vector<Interval> intervals = {{1, 2}, {4, 5}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        variant = dis;
        REQUIRE(operation.canApply(variant));

        // Null pointer
        std::shared_ptr<DigitalIntervalSeries> null_ptr = nullptr;
        variant = null_ptr;
        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("Default parameters") {
        auto default_params = operation.getDefaultParameters();
        REQUIRE(default_params != nullptr);

        auto * group_params = dynamic_cast<GroupParams *>(default_params.get());
        REQUIRE(group_params != nullptr);
        REQUIRE(group_params->maxSpacing == 1.0);
    }

    SECTION("execute with valid input") {
        std::vector<Interval> intervals = {{1, 2}, {4, 5}, {10, 11}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        variant = dis;

        params.maxSpacing = 3.0;

        auto result = operation.execute(variant, &params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        auto result_dis = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_dis != nullptr);

        auto const & grouped = result_dis->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 2);
        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 5);
        REQUIRE(grouped[1].start == 10);
        REQUIRE(grouped[1].end == 11);
    }

    SECTION("execute with progress callback") {
        std::vector<Interval> intervals = {{1, 2}, {4, 5}, {10, 11}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        variant = dis;

        int volatile progress_val = -1;
        int volatile call_count = 0;
        ProgressCallback cb = [&](int p) {
            progress_val = p;
            call_count = call_count + 1;
        };

        auto result = operation.execute(variant, &params, cb);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        auto result_dis = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_dis != nullptr);
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }

    SECTION("execute with wrong parameter type") {
        std::vector<Interval> intervals = {{1, 2}, {4, 5}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        variant = dis;

        // Create a different parameter type
        struct WrongParams : public TransformParametersBase {
            int dummy = 42;
        } wrong_params;

        auto result = operation.execute(variant, &wrong_params);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        // Should still work with default parameters
        auto result_dis = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_dis != nullptr);
    }

    SECTION("execute with null parameters") {
        std::vector<Interval> intervals = {{1, 2}, {4, 5}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        variant = dis;

        auto result = operation.execute(variant, nullptr);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result));

        auto result_dis = std::get<std::shared_ptr<DigitalIntervalSeries>>(result);
        REQUIRE(result_dis != nullptr);
    }
}

TEST_CASE("Group Transform Edge Cases", "[transforms][digital_interval_group][edge_cases]") {
    GroupParams params;

    SECTION("Fractional spacing") {
        std::vector<Interval> intervals = {{1, 2}, {4, 5}, {7, 8}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 1.5;// Between 1 and 2

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();

        // All intervals should group into one since all gaps are ≤ 1.5
        // Gap between (1,2) and (4,5) is 1 ≤ 1.5, and gap between (4,5) and (7,8) is 1 ≤ 1.5
        REQUIRE(grouped.size() == 1);
        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 8);
    }

    SECTION("Negative spacing") {
        std::vector<Interval> intervals = {{1, 2}, {4, 5}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = -1.0;// Negative spacing

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 2);// No grouping with negative spacing

        REQUIRE(grouped[0].start == 1);
        REQUIRE(grouped[0].end == 2);
        REQUIRE(grouped[1].start == 4);
        REQUIRE(grouped[1].end == 5);
    }

    SECTION("Very large intervals") {
        std::vector<Interval> intervals = {{1000000, 2000000}, {3000000, 4000000}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 1000000.0;// Exactly the gap size

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 1);// Should group (gap = 999999 ≤ 1000000)

        REQUIRE(grouped[0].start == 1000000);
        REQUIRE(grouped[0].end == 4000000);
    }

    SECTION("Many small intervals") {
        std::vector<Interval> intervals;
        for (int i = 0; i < 100; ++i) {
            intervals.push_back({i * 3, i * 3 + 1});// Intervals with gaps of 1
        }
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.maxSpacing = 1.0;// Should group all

        auto result = group_intervals(dis.get(), params);
        REQUIRE(result != nullptr);

        auto const & grouped = result->getDigitalIntervalSeries();
        REQUIRE(grouped.size() == 1);

        REQUIRE(grouped[0].start == 0);
        REQUIRE(grouped[0].end == 99 * 3 + 1);
    }
}