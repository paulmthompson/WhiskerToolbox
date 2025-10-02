#include "DataManager/transforms/DigitalIntervalSeries/Digital_Interval_Invert/digital_interval_invert.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/TimeFrame/interval_data.hpp"

#include <gtest/gtest.h>
#include <vector>

class InvertIntervalTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic inversion with unbounded domain
TEST_F(InvertIntervalTest, UnboundedInversion) {
    // Create test intervals: (5,10), (13,20), (23,40), (56,70), (72,91)
    std::vector<Interval> input_intervals = {
        {5, 10}, {13, 20}, {23, 40}, {56, 70}, {72, 91}
    };

    auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

    InvertParams params;
    params.domainType = DomainType::Unbounded;

    auto result = invert_intervals(input_series.get(), params);

    ASSERT_NE(result, nullptr);

    auto result_intervals = result->getDigitalIntervalSeries();

    // Expected: (10,13), (20,23), (40,56), (70,72)
    std::vector<Interval> expected = {
        {11, 12}, {21, 22}, {41, 55}, {71, 71}
    };

    EXPECT_EQ(result_intervals.size(), 4);
    EXPECT_EQ(result_intervals[0].start, 11);
    EXPECT_EQ(result_intervals[0].end, 12);
    EXPECT_EQ(result_intervals[1].start, 21);
    EXPECT_EQ(result_intervals[1].end, 22);
    EXPECT_EQ(result_intervals[2].start, 41);
    EXPECT_EQ(result_intervals[2].end, 55);
    EXPECT_EQ(result_intervals[3].start, 71);
    EXPECT_EQ(result_intervals[3].end, 71);
}

// Test bounded inversion
TEST_F(InvertIntervalTest, BoundedInversion) {
    // Create test intervals: (5,10), (13,20), (23,40), (56,70), (72,91)
    std::vector<Interval> input_intervals = {
        {5, 10}, {13, 20}, {23, 40}, {56, 70}, {72, 91}
    };

    auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

    InvertParams params;
    params.domainType = DomainType::Bounded;
    params.boundStart = 0.0;
    params.boundEnd = 100.0;

    auto result = invert_intervals(input_series.get(), params);

    ASSERT_NE(result, nullptr);

    auto result_intervals = result->getDigitalIntervalSeries();

    // Expected: (0,5), (10,13), (20,23), (40,56), (70,72), (91,100)
    EXPECT_EQ(result_intervals.size(), 6);
    EXPECT_EQ(result_intervals[0].start, 0);
    EXPECT_EQ(result_intervals[0].end, 4);
    EXPECT_EQ(result_intervals[1].start, 11);
    EXPECT_EQ(result_intervals[1].end, 12);
    EXPECT_EQ(result_intervals[2].start, 21);
    EXPECT_EQ(result_intervals[2].end, 22);
    EXPECT_EQ(result_intervals[3].start, 41);
    EXPECT_EQ(result_intervals[3].end, 55);
    EXPECT_EQ(result_intervals[4].start, 71);
    EXPECT_EQ(result_intervals[4].end, 71);
    EXPECT_EQ(result_intervals[5].start, 92);
    EXPECT_EQ(result_intervals[5].end, 100);
}

// Test empty input with unbounded domain
TEST_F(InvertIntervalTest, EmptyInputUnbounded) {
    std::vector<Interval> input_intervals = {};
    auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

    InvertParams params;
    params.domainType = DomainType::Unbounded;

    auto result = invert_intervals(input_series.get(), params);

    ASSERT_NE(result, nullptr);
    auto result_intervals = result->getDigitalIntervalSeries();
    EXPECT_EQ(result_intervals.size(), 0);
}

// Test empty input with bounded domain
TEST_F(InvertIntervalTest, EmptyInputBounded) {
    std::vector<Interval> input_intervals = {};
    auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

    InvertParams params;
    params.domainType = DomainType::Bounded;
    params.boundStart = 0.0;
    params.boundEnd = 100.0;

    auto result = invert_intervals(input_series.get(), params);

    ASSERT_NE(result, nullptr);
    auto result_intervals = result->getDigitalIntervalSeries();

    // Should return the entire domain as one interval
    EXPECT_EQ(result_intervals.size(), 1);
    EXPECT_EQ(result_intervals[0].start, 0);
    EXPECT_EQ(result_intervals[0].end, 100);
}

// Test single interval
TEST_F(InvertIntervalTest, SingleInterval) {
    std::vector<Interval> input_intervals = {{10, 20}};
    auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

    InvertParams params;
    params.domainType = DomainType::Bounded;
    params.boundStart = 0.0;
    params.boundEnd = 30.0;

    auto result = invert_intervals(input_series.get(), params);

    ASSERT_NE(result, nullptr);
    auto result_intervals = result->getDigitalIntervalSeries();

    // Expected: (0,9), (21,30)
    EXPECT_EQ(result_intervals.size(), 2);
    EXPECT_EQ(result_intervals[0].start, 0);
    EXPECT_EQ(result_intervals[0].end, 9);
    EXPECT_EQ(result_intervals[1].start, 21);
    EXPECT_EQ(result_intervals[1].end, 30);
}

// Test adjacent intervals (no gaps)
TEST_F(InvertIntervalTest, AdjacentIntervals) {
    std::vector<Interval> input_intervals = {{5, 10}, {11, 20}};
    auto input_series = std::make_shared<DigitalIntervalSeries>(input_intervals);

    InvertParams params;
    params.domainType = DomainType::Unbounded;

    auto result = invert_intervals(input_series.get(), params);

    ASSERT_NE(result, nullptr);
    auto result_intervals = result->getDigitalIntervalSeries();

    // No gaps between intervals, so result should be empty
    EXPECT_EQ(result_intervals.size(), 0);
}

// Test InvertIntervalOperation class
TEST_F(InvertIntervalTest, OperationInterface) {
    InvertIntervalOperation operation;

    // Test name
    EXPECT_EQ(operation.getName(), "Invert Intervals");

    // Test target type index
    EXPECT_EQ(operation.getTargetInputTypeIndex(), typeid(std::shared_ptr<DigitalIntervalSeries>));

    // Test default parameters
    auto default_params = operation.getDefaultParameters();
    ASSERT_NE(default_params, nullptr);

    auto* invert_params = dynamic_cast<InvertParams*>(default_params.get());
    ASSERT_NE(invert_params, nullptr);
    EXPECT_EQ(invert_params->domainType, DomainType::Unbounded);
    EXPECT_EQ(invert_params->boundStart, 0.0);
    EXPECT_EQ(invert_params->boundEnd, 100.0);
}

// Test null input handling
TEST_F(InvertIntervalTest, NullInput) {
    InvertParams params;
    params.domainType = DomainType::Unbounded;

    auto result = invert_intervals(nullptr, params);

    ASSERT_NE(result, nullptr);
    auto result_intervals = result->getDigitalIntervalSeries();
    EXPECT_EQ(result_intervals.size(), 0);
}
