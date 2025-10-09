#include "line_base_flip.hpp"
#include "Lines/Line_Data.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <gtest/gtest.h>
#include <memory>

class LineBaseFlipTest : public ::testing::Test {
protected:
    void SetUp() override {
        transform = std::make_unique<LineBaseFlipTransform>();
        line_data = std::make_shared<LineData>();
    }

    std::unique_ptr<LineBaseFlipTransform> transform;
    std::shared_ptr<LineData> line_data;
};

TEST_F(LineBaseFlipTest, CanApplyToLineData) {
    DataTypeVariant variant = line_data;
    EXPECT_TRUE(transform->canApply(variant));
}

TEST_F(LineBaseFlipTest, CannotApplyToNonLineData) {
    DataTypeVariant variant = std::make_shared<int>(42);
    EXPECT_FALSE(transform->canApply(variant));
}

TEST_F(LineBaseFlipTest, FlipLineBasedOnReferencePoint) {
    // Create a simple line from (0,0) to (10,0)
    Line2D original_line;
    original_line.push_back(Point2D<float>{0.0f, 0.0f});  // Base point
    original_line.push_back(Point2D<float>{5.0f, 0.0f});
    original_line.push_back(Point2D<float>{10.0f, 0.0f}); // End point

    // Add line to frame 0
    line_data->addAtTime(0, original_line);

    // Set reference point closer to the end (10,0) than the base (0,0)
    Point2D<float> reference_point{12.0f, 0.0f};
    LineBaseFlipParameters params(reference_point);

    // Execute transform
    DataTypeVariant input_variant = line_data;
    auto result_variant = transform->execute(input_variant, &params);

    // Verify result
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));
    auto result_data = std::get<std::shared_ptr<LineData>>(result_variant);

    auto const & result_lines = result_data->getAtTime(0);
    ASSERT_EQ(result_lines.size(), 1);

    auto const & flipped_line = result_lines[0];
    ASSERT_EQ(flipped_line.size(), 3);

    // Check that the line has been flipped (base should now be at (10,0))
    EXPECT_FLOAT_EQ(flipped_line.front().x, 10.0f);
    EXPECT_FLOAT_EQ(flipped_line.front().y, 0.0f);
    EXPECT_FLOAT_EQ(flipped_line.back().x, 0.0f);
    EXPECT_FLOAT_EQ(flipped_line.back().y, 0.0f);
}

TEST_F(LineBaseFlipTest, DoNotFlipWhenBaseIsCloser) {
    // Create a simple line from (0,0) to (10,0)
    Line2D original_line;
    original_line.push_back(Point2D<float>{0.0f, 0.0f});  // Base point
    original_line.push_back(Point2D<float>{5.0f, 0.0f});
    original_line.push_back(Point2D<float>{10.0f, 0.0f}); // End point

    // Add line to frame 0
    line_data->addAtTime(0, original_line);

    // Set reference point closer to the base (0,0) than the end (10,0)
    Point2D<float> reference_point{-2.0f, 0.0f};
    LineBaseFlipParameters params(reference_point);

    // Execute transform
    DataTypeVariant input_variant = line_data;
    auto result_variant = transform->execute(input_variant, &params);

    // Verify result
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));
    auto result_data = std::get<std::shared_ptr<LineData>>(result_variant);

    auto const & result_lines = result_data->getAtTime(0);
    ASSERT_EQ(result_lines.size(), 1);

    auto const & unchanged_line = result_lines[0];
    ASSERT_EQ(unchanged_line.size(), 3);

    // Check that the line has NOT been flipped (base should still be at (0,0))
    EXPECT_FLOAT_EQ(unchanged_line.front().x, 0.0f);
    EXPECT_FLOAT_EQ(unchanged_line.front().y, 0.0f);
    EXPECT_FLOAT_EQ(unchanged_line.back().x, 10.0f);
    EXPECT_FLOAT_EQ(unchanged_line.back().y, 0.0f);
}

TEST_F(LineBaseFlipTest, HandleEmptyLine) {
    // Create an empty line
    Line2D empty_line;

    // Add line to frame 0
    line_data->addAtTime(0, empty_line);

    Point2D<float> reference_point{5.0f, 5.0f};
    LineBaseFlipParameters params(reference_point);

    // Execute transform
    DataTypeVariant input_variant = line_data;
    auto result_variant = transform->execute(input_variant, &params);

    // Verify result - should not crash and should return unchanged data
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));
    auto result_data = std::get<std::shared_ptr<LineData>>(result_variant);

    auto const & result_lines = result_data->getAtTime(0);
    ASSERT_EQ(result_lines.size(), 1);
    EXPECT_TRUE(result_lines[0].empty());
}

TEST_F(LineBaseFlipTest, HandleSinglePointLine) {
    // Create a line with only one point
    Line2D single_point_line;
    single_point_line.push_back(Point2D<float>{5.0f, 5.0f});

    // Add line to frame 0
    line_data->addAtTime(0, single_point_line);

    Point2D<float> reference_point{0.0f, 0.0f};
    LineBaseFlipParameters params(reference_point);

    // Execute transform
    DataTypeVariant input_variant = line_data;
    auto result_variant = transform->execute(input_variant, &params);

    // Verify result - should not flip single point line
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));
    auto result_data = std::get<std::shared_ptr<LineData>>(result_variant);

    auto const & result_lines = result_data->getAtTime(0);
    ASSERT_EQ(result_lines.size(), 1);
    ASSERT_EQ(result_lines[0].size(), 1);
    EXPECT_FLOAT_EQ(result_lines[0].front().x, 5.0f);
    EXPECT_FLOAT_EQ(result_lines[0].front().y, 5.0f);
}

TEST_F(LineBaseFlipTest, ProcessMultipleFrames) {
    // Create lines for multiple frames
    Line2D line1;
    line1.push_back(Point2D<float>{0.0f, 0.0f});
    line1.push_back(Point2D<float>{10.0f, 0.0f});

    Line2D line2;
    line2.push_back(Point2D<float>{0.0f, 10.0f});
    line2.push_back(Point2D<float>{10.0f, 10.0f});

    // Add lines to different frames
    line_data->addAtTime(0, line1);
    line_data->addAtTime(1, line2);

    // Reference point closer to end points
    Point2D<float> reference_point{12.0f, 5.0f};
    LineBaseFlipParameters params(reference_point);

    // Execute transform
    DataTypeVariant input_variant = line_data;
    auto result_variant = transform->execute(input_variant, &params);

    // Verify both frames were processed
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));
    auto result_data = std::get<std::shared_ptr<LineData>>(result_variant);

    // Check frame 0
    auto const & frame0_lines = result_data->getAtTime(0);
    ASSERT_EQ(frame0_lines.size(), 1);
    EXPECT_FLOAT_EQ(frame0_lines[0].front().x, 10.0f); // Should be flipped

    // Check frame 1
    auto const & frame1_lines = result_data->getAtTime(1);
    ASSERT_EQ(frame1_lines.size(), 1);
    EXPECT_FLOAT_EQ(frame1_lines[0].front().x, 10.0f); // Should be flipped
}
