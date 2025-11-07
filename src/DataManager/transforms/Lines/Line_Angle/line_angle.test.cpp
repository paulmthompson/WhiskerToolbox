#include "transforms/Lines/Line_Angle/line_angle.hpp"
#include "Lines/Line_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include <cmath>
#include <nlohmann/json.hpp>

TEST_CASE("Line angle calculation - Core functionality", "[line][angle][transform]") {
    auto line_data = std::make_shared<LineData>();

    SECTION("Calculating angle from empty line data") {
        auto params = std::make_unique<LineAngleParameters>();
        auto result = line_angle(line_data.get(), params.get());

        REQUIRE(result->getAnalogTimeSeries().empty());
        REQUIRE(result->getTimeSeries().empty());
    }

    SECTION("Direct angle calculation - Horizontal line") {
        // Create a horizontal line
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {1.0f, 1.0f, 1.0f, 1.0f};
        line_data->emplaceAtTime(TimeFrameIndex(10), x_coords, y_coords);

        // Position at 33% (approx. between points 1 and 2)
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.33f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(10));
        // Angle should be 0 degrees (horizontal line points right)
        REQUIRE_THAT(values[0], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Direct angle calculation - Vertical line") {
        // Create a vertical line pointing up
        std::vector<float> x_coords = {1.0f, 1.0f, 1.0f, 1.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(20), x_coords, y_coords);

        // Position at 25% (at point 1)
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.25f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(20));
        // Angle should be 90 degrees (vertical line points up)
        REQUIRE_THAT(values[0], Catch::Matchers::WithinAbs(90.0f, 0.001f));
    }

    SECTION("Direct angle calculation - Diagonal line (45 degrees)") {
        // Create a diagonal line (45 degrees)
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(30), x_coords, y_coords);

        // Position at 50% (at point 2)
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.50f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(30));
        // Angle should be 45 degrees
        REQUIRE_THAT(values[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Direct angle calculation - Multiple time points") {
        // Create lines at different time points with different angles

        // Horizontal line at time 40
        std::vector<float> x1 = {0.0f, 1.0f, 2.0f};
        std::vector<float> y1 = {1.0f, 1.0f, 1.0f};
        line_data->emplaceAtTime(TimeFrameIndex(40), x1, y1);

        // Vertical line at time 50
        std::vector<float> x2 = {1.0f, 1.0f, 1.0f};
        std::vector<float> y2 = {0.0f, 1.0f, 2.0f};
        line_data->emplaceAtTime(TimeFrameIndex(50), x2, y2);

        // 45-degree line at time 60
        std::vector<float> x3 = {0.0f, 1.0f, 2.0f};
        std::vector<float> y3 = {0.0f, 1.0f, 2.0f};
        line_data->emplaceAtTime(TimeFrameIndex(60), x3, y3);

        // Position at 50%
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->method = AngleCalculationMethod::DirectPoints;

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 3);
        REQUIRE(values.size() == 3);

        // Find time indices and check angles
        auto time40_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(40)));
        auto time50_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(50)));
        auto time60_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), TimeFrameIndex(60)));

        // Horizontal line: 0 degrees
        REQUIRE_THAT(values[time40_idx], Catch::Matchers::WithinAbs(0.0f, 0.001f));
        // Vertical line: 90 degrees
        REQUIRE_THAT(values[time50_idx], Catch::Matchers::WithinAbs(90.0f, 0.001f));
        // 45-degree line: 45 degrees
        REQUIRE_THAT(values[time60_idx], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Polynomial angle calculation - Simple line") {
        // Create a curve (points on a parabola)
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 4.0f, 9.0f, 16.0f, 25.0f};
        line_data->emplaceAtTime(TimeFrameIndex(70), x_coords, y_coords);

        // Position at 40% with polynomial fitting
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.4f; // Around x=3 (37% of length of line)
        params->method = AngleCalculationMethod::PolynomialFit;
        params->polynomial_order = 2; // Use a quadratic fit for this parabola

        auto result = line_angle(line_data.get(), params.get());

        auto const & values = result->getAnalogTimeSeries();
        auto const & times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(70));

        // For a parabola y = x², the derivative is 2x. So slope at x=3 is 6
        // Angle of atan(6,1) is approximately 80.537 degrees. 
        REQUIRE(values[0] > 75.0f);
        REQUIRE(values[0] < 85.0f);
    }

    SECTION("Different polynomial orders") {
        // Create a smooth curve
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
        std::vector<float> y_coords = {0.0f, 0.5f, 1.8f, 3.9f, 6.8f, 10.5f, 15.0f, 20.3f};
        line_data->emplaceAtTime(TimeFrameIndex(80), x_coords, y_coords);

        // Test with different polynomial orders
        auto position = 0.5f; // Middle of the line

        // First order (linear fit)
        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = position;
        params1->method = AngleCalculationMethod::PolynomialFit;
        params1->polynomial_order = 1;
        auto result1 = line_angle(line_data.get(), params1.get());

        // Third order
        auto params3 = std::make_unique<LineAngleParameters>();
        params3->position = position;
        params3->method = AngleCalculationMethod::PolynomialFit;
        params3->polynomial_order = 3;
        auto result3 = line_angle(line_data.get(), params3.get());

        // Fifth order
        auto params5 = std::make_unique<LineAngleParameters>();
        params5->position = position;
        params5->method = AngleCalculationMethod::PolynomialFit;
        params5->polynomial_order = 5;
        auto result5 = line_angle(line_data.get(), params5.get());

        // Each result should have proper data structure
        REQUIRE(result1->getTimeSeries().size() == 1);
        REQUIRE(result3->getTimeSeries().size() == 1);
        REQUIRE(result5->getTimeSeries().size() == 1);

        // Higher order polynomials should better capture local curvature
        // For this curve, we expect higher orders to yield different angles
        // from lower orders (exact values will depend on the curve)
        auto angle1 = result1->getAnalogTimeSeries()[0];
        auto angle3 = result3->getAnalogTimeSeries()[0];
        auto angle5 = result5->getAnalogTimeSeries()[0];

        // We don't check exact values, but ensure they're reasonable angles
        REQUIRE(angle1 >= -180.0f);
        REQUIRE(angle1 <= 180.0f);
        REQUIRE(angle3 >= -180.0f);
        REQUIRE(angle3 <= 180.0f);
        REQUIRE(angle5 >= -180.0f);
        REQUIRE(angle5 <= 180.0f);

        // The angles should be different because of the different polynomial orders
        REQUIRE((std::abs(angle1 - angle3) > 1.0f || std::abs(angle1 - angle5) > 1.0f));
    }

    SECTION("Verify returned AnalogTimeSeries structure") {
        // Add a simple line
        std::vector<float> x = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y = {0.0f, 0.0f, 0.0f, 0.0f};
        line_data->emplaceAtTime(TimeFrameIndex(100), x, y);

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        auto result = line_angle(line_data.get(), params.get());

        // Verify it's a proper AnalogTimeSeries
        REQUIRE(result != nullptr);
        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);

        // Statistics should work on the result
        float angle = result->getAnalogTimeSeries()[0];
        REQUIRE(calculate_mean(*result.get()) == angle);
        REQUIRE(calculate_min(*result.get()) == angle);
        REQUIRE(calculate_max(*result.get()) == angle);
    }

    SECTION("Reference vector - Horizontal reference") {
        // Create a 45-degree line
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(110), x_coords, y_coords);

        // Default reference (1,0) - horizontal reference
        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = 0.5f;
        params1->reference_x = 1.0f;
        params1->reference_y = 0.0f;
        auto result1 = line_angle(line_data.get(), params1.get());

        // The angle should be 45 degrees (default reference is horizontal)
        REQUIRE_THAT(result1->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Reference vector - Vertical reference") {
        // Create a 45-degree line
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(120), x_coords, y_coords);

        // Vertical reference (0,1)
        auto params2 = std::make_unique<LineAngleParameters>();
        params2->position = 0.5f;
        params2->reference_x = 0.0f;
        params2->reference_y = 1.0f;
        auto result2 = line_angle(line_data.get(), params2.get());

        // The angle should be -45 degrees (angle from vertical to 45-degree line)
        REQUIRE_THAT(result2->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(-45.0f, 0.001f));
    }

    SECTION("Reference vector - 45-degree reference") {
        // Create a horizontal line
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {1.0f, 1.0f, 1.0f, 1.0f};
        line_data->emplaceAtTime(TimeFrameIndex(130), x_coords, y_coords);

        // 45-degree reference (1,1)
        auto params3 = std::make_unique<LineAngleParameters>();
        params3->position = 0.5f;
        params3->reference_x = 1.0f;
        params3->reference_y = 1.0f;
        auto result3 = line_angle(line_data.get(), params3.get());

        // The angle should be -45 degrees (angle from 45-degree to horizontal)
        REQUIRE_THAT(result3->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(-45.0f, 0.001f));
    }

    SECTION("Reference vector with polynomial fit") {
        // Create a parabolic curve
        std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<float> y_coords = {0.0f, 1.0f, 4.0f, 9.0f, 16.0f};
        line_data->emplaceAtTime(TimeFrameIndex(140), x_coords, y_coords);

        // Use default reference (1,0) with polynomial fit
        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = 0.5f;
        params1->method = AngleCalculationMethod::PolynomialFit;
        params1->polynomial_order = 2;
        auto result1 = line_angle(line_data.get(), params1.get());

        // Now use vertical reference (0,1) with same polynomial fit
        auto params2 = std::make_unique<LineAngleParameters>();
        params2->position = 0.5f;
        params2->method = AngleCalculationMethod::PolynomialFit;
        params2->polynomial_order = 2;
        params2->reference_x = 0.0f;
        params2->reference_y = 1.0f;
        auto result2 = line_angle(line_data.get(), params2.get());

        // The difference between the two angles should be approximately 90 degrees
        float angle1 = result1->getAnalogTimeSeries()[0];
        float angle2 = result2->getAnalogTimeSeries()[0];

        // Adjust for angle wrapping
        float angle_diff = angle1 - angle2;
        if (angle_diff > 180.0f) angle_diff -= 360.0f;
        if (angle_diff <= -180.0f) angle_diff += 360.0f;

        REQUIRE_THAT(std::abs(angle_diff), Catch::Matchers::WithinAbs(90.0f, 5.0f));
    }
}

TEST_CASE("Line angle calculation - Edge cases and error handling", "[line][angle][transform][edge]") {
    auto line_data = std::make_shared<LineData>();

    SECTION("Line with only one point") {
        // Add a line with just one point (should skip this line)
        std::vector<float> x = {1.0f};
        std::vector<float> y = {1.0f};
        line_data->emplaceAtTime(TimeFrameIndex(10), x, y);

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        auto result = line_angle(line_data.get(), params.get());

        // Should return an empty result
        REQUIRE(result->getAnalogTimeSeries().empty());
        REQUIRE(result->getTimeSeries().empty());
    }

    SECTION("Position out of range") {
        // Create a normal line
        std::vector<float> x = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(20), x, y);

        // Test position below 0
        auto params_low = std::make_unique<LineAngleParameters>();
        params_low->position = -0.5f;  // Should clamp to 0.0
        auto result_low = line_angle(line_data.get(), params_low.get());

        // Test position above 1
        auto params_high = std::make_unique<LineAngleParameters>();
        params_high->position = 1.5f;  // Should clamp to 1.0
        auto result_high = line_angle(line_data.get(), params_high.get());

        // Both should work and use clamped positions
        REQUIRE(result_low->getAnalogTimeSeries().size() == 1);
        REQUIRE(result_high->getAnalogTimeSeries().size() == 1);

        // Low position (0.0) should be the angle from the first point to itself, which is not defined
        // but implementation should handle this gracefully
        float low_angle = result_low->getAnalogTimeSeries()[0];
        REQUIRE((std::isnan(low_angle) || (-180.0f <= low_angle && low_angle <= 180.0f)));

        // High position (1.0) should be the angle from first to last point (45 degrees)
        float high_angle = result_high->getAnalogTimeSeries()[0];
        REQUIRE_THAT(high_angle, Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Multiple lines at same timestamp") {
        // Create two lines at the same timestamp
        std::vector<float> x1 = {0.0f, 1.0f, 2.0f};
        std::vector<float> y1 = {0.0f, 0.0f, 0.0f};  // Horizontal: 0 degrees
        line_data->emplaceAtTime(TimeFrameIndex(30), x1, y1);

        std::vector<float> x2 = {0.0f, 0.0f, 0.0f};
        std::vector<float> y2 = {0.0f, 1.0f, 2.0f};  // Vertical: 90 degrees
        line_data->emplaceAtTime(TimeFrameIndex(30), x2, y2);

        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        auto result = line_angle(line_data.get(), params.get());

        // Only the first line should be used
        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Polynomial fit with too few points") {
        // Create a line with fewer points than polynomial order
        std::vector<float> x = {0.0f, 1.0f};
        std::vector<float> y = {0.0f, 1.0f};
        line_data->emplaceAtTime(TimeFrameIndex(40), x, y);

        // Try to fit a 3rd order polynomial (requires at least 4 points)
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->method = AngleCalculationMethod::PolynomialFit;
        params->polynomial_order = 3;
        auto result = line_angle(line_data.get(), params.get());

        // Should fall back to direct method instead of failing
        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        // 45 degree angle with direct method
        REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Polynomial fit with collinear points") {
        // Create a vertical line where x values are all the same
        std::vector<float> x = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        std::vector<float> y = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        line_data->emplaceAtTime(TimeFrameIndex(50), x, y);

        // Try polynomial fit which may be numerically unstable
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->method = AngleCalculationMethod::PolynomialFit;
        params->polynomial_order = 3;
        auto result = line_angle(line_data.get(), params.get());

        // Should still produce a result (either falling back to direct method or
        // producing a reasonable angle through the polynomial fit)
        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);

        float angle = result->getAnalogTimeSeries()[0];
        // Should be close to 90 degrees (vertical line)
        REQUIRE(((angle > 80.0f && angle < 100.0f) || (angle < -80.0f && angle > -100.0f)));
    }

    SECTION("Null parameters") {
        // Create a simple line
        std::vector<float> x = {0.0f, 1.0f, 2.0f};
        std::vector<float> y = {0.0f, 1.0f, 2.0f};
        line_data->emplaceAtTime(TimeFrameIndex(60), x, y);

        // Call with null parameters
        auto result = line_angle(line_data.get(), nullptr);

        // Should use default parameters
        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        // Default is 0.2 position with direct method, expect about 45 degrees
        REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Large number of points") {
        // Create a line with many points
        std::vector<float> x;
        std::vector<float> y;
        for (int i = 0; i < 1000; ++i) {
            x.push_back(static_cast<float>(i));
            y.push_back(static_cast<float>(i));  // 45-degree line
        }
        line_data->emplaceAtTime(TimeFrameIndex(70), x, y);

        // Test both methods
        auto params_direct = std::make_unique<LineAngleParameters>();
        params_direct->position = 0.5f;
        params_direct->method = AngleCalculationMethod::DirectPoints;
        auto result_direct = line_angle(line_data.get(), params_direct.get());

        auto params_poly = std::make_unique<LineAngleParameters>();
        params_poly->position = 0.5f;
        params_poly->method = AngleCalculationMethod::PolynomialFit;
        params_poly->polynomial_order = 3;
        auto result_poly = line_angle(line_data.get(), params_poly.get());

        // Both should handle large number of points
        REQUIRE(result_direct->getAnalogTimeSeries().size() == 1);
        REQUIRE(result_poly->getAnalogTimeSeries().size() == 1);

        // Both should produce close to 45 degrees
        REQUIRE_THAT(result_direct->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
        REQUIRE_THAT(result_poly->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 1.0f));
    }

    SECTION("Zero reference vector") {
        // Create a simple line
        std::vector<float> x = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y = {0.0f, 1.0f, 2.0f, 3.0f};
        line_data->emplaceAtTime(TimeFrameIndex(80), x, y);

        // Try a zero reference vector (should default to (1,0))
        auto params = std::make_unique<LineAngleParameters>();
        params->position = 0.5f;
        params->reference_x = 0.0f;
        params->reference_y = 0.0f;
        auto result = line_angle(line_data.get(), params.get());

        // The angle should be the same as with the default reference
        REQUIRE_THAT(result->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
    }

    SECTION("Normalizing reference vector") {
        // Create a simple line
        std::vector<float> x = {0.0f, 1.0f, 2.0f, 3.0f};
        std::vector<float> y = {0.0f, 0.0f, 0.0f, 0.0f};
        line_data->emplaceAtTime(TimeFrameIndex(90), x, y);

        // Use an unnormalized reference vector
        auto params1 = std::make_unique<LineAngleParameters>();
        params1->position = 0.5f;
        params1->reference_x = 0.0f;
        params1->reference_y = 2.0f;  // Magnitude of 2
        auto result1 = line_angle(line_data.get(), params1.get());

        // Use the same reference vector but normalized
        auto params2 = std::make_unique<LineAngleParameters>();
        params2->position = 0.5f;
        params2->reference_x = 0.0f;
        params2->reference_y = 1.0f;  // Normalized to magnitude of 1
        auto result2 = line_angle(line_data.get(), params2.get());

        // Both should give the same angle (reference direction is what matters, not magnitude)
        REQUIRE_THAT(result1->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(result2->getAnalogTimeSeries()[0], 0.001f));
    }

    SECTION("Specific problematic 2-point lines with negative reference vector") {
        // Test Line 1: (565, 253), (408, 277)
        std::vector<float> x1 = {565.0f, 408.0f};
        std::vector<float> y1 = {253.0f, 277.0f};
        line_data->emplaceAtTime(TimeFrameIndex(200), x1, y1);

        // Test Line 2: (567, 252), (434, 265)
        std::vector<float> x2 = {567.0f, 434.0f};
        std::vector<float> y2 = {252.0f, 265.0f};
        line_data->emplaceAtTime(TimeFrameIndex(210), x2, y2);

        // Test with reference vector (-1, 0) at 80% position
        auto params_80 = std::make_unique<LineAngleParameters>();
        params_80->position = 0.8f;
        params_80->reference_x = -1.0f;
        params_80->reference_y = 0.0f;
        params_80->method = AngleCalculationMethod::DirectPoints;
        auto result_80 = line_angle(line_data.get(), params_80.get());

        // Test with reference vector (-1, 0) at 100% position
        auto params_100 = std::make_unique<LineAngleParameters>();
        params_100->position = 1.0f;
        params_100->reference_x = -1.0f;
        params_100->reference_y = 0.0f;
        params_100->method = AngleCalculationMethod::DirectPoints;
        auto result_100 = line_angle(line_data.get(), params_100.get());

        // Verify we get results for both lines
        REQUIRE(result_80->getAnalogTimeSeries().size() == 2);
        REQUIRE(result_80->getTimeSeries().size() == 2);
        REQUIRE(result_100->getAnalogTimeSeries().size() == 2);
        REQUIRE(result_100->getTimeSeries().size() == 2);

        // Check that results are not +/- 180 degrees (the problematic values)
        for (size_t i = 0; i < result_80->getAnalogTimeSeries().size(); ++i) {
            float angle_80 = result_80->getAnalogTimeSeries()[i];
            float angle_100 = result_100->getAnalogTimeSeries()[i];
            
            // Results should not be exactly 180 or -180
            REQUIRE(angle_80 != 180.0f);
            REQUIRE(angle_80 != -180.0f);
            REQUIRE(angle_100 != 180.0f);
            REQUIRE(angle_100 != -180.0f);
            
            // Results should be within valid angle range
            REQUIRE(angle_80 >= -180.0f);
            REQUIRE(angle_80 <= 180.0f);
            REQUIRE(angle_100 >= -180.0f);
            REQUIRE(angle_100 <= 180.0f);
            
            // Print the actual values for debugging
            std::cout << "Line " << (i+1) << " at 80%: " << angle_80 << " degrees" << std::endl;
            std::cout << "Line " << (i+1) << " at 100%: " << angle_100 << " degrees" << std::endl;
        }

        // Test with polynomial fit method as well
        auto params_poly_80 = std::make_unique<LineAngleParameters>();
        params_poly_80->position = 0.8f;
        params_poly_80->reference_x = -1.0f;
        params_poly_80->reference_y = 0.0f;
        params_poly_80->method = AngleCalculationMethod::PolynomialFit;
        params_poly_80->polynomial_order = 1; // Linear fit for 2 points
        auto result_poly_80 = line_angle(line_data.get(), params_poly_80.get());

        // Polynomial fit should fall back to direct method for 2 points
        REQUIRE(result_poly_80->getAnalogTimeSeries().size() == 2);
        for (size_t i = 0; i < result_poly_80->getAnalogTimeSeries().size(); ++i) {
            float angle_poly = result_poly_80->getAnalogTimeSeries()[i];
            REQUIRE(angle_poly != 180.0f);
            REQUIRE(angle_poly != -180.0f);
            std::cout << "Line " << (i+1) << " polynomial at 80%: " << angle_poly << " degrees" << std::endl;
        }
    }
}

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Line Angle - JSON pipeline", "[transforms][line_angle][json]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "line_angle_step_1"},
            {"transform_name", "Calculate Line Angle"},
            {"input_key", "TestLine.line1"},
            {"output_key", "LineAngles"},
            {"parameters", {
                {"position", 0.5},
                {"method", "Direct Points"},
                {"polynomial_order", 3},
                {"reference_x", 1.0},
                {"reference_y", 0.0}
            }}
        }}}
    };

    DataManager dm;
    TransformRegistry registry;

    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data - 45-degree line
    auto line_data = std::make_shared<LineData>();
    std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> y_coords = {0.0f, 1.0f, 2.0f, 3.0f};
    line_data->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
    line_data->setTimeFrame(time_frame);
    dm.setData("TestLine.line1", line_data, TimeKey("default"));

    TransformPipeline pipeline(&dm, &registry);
    pipeline.loadFromJson(json_config);
    pipeline.execute();

    // Verify the results
    auto angle_series = dm.getData<AnalogTimeSeries>("LineAngles");
    REQUIRE(angle_series != nullptr);
    REQUIRE(angle_series->getAnalogTimeSeries().size() == 1);
    REQUIRE(angle_series->getTimeSeries().size() == 1);
    REQUIRE(angle_series->getTimeSeries()[0] == TimeFrameIndex(100));
    // Should be 45 degrees for a 45-degree line
    REQUIRE_THAT(angle_series->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(45.0f, 0.001f));
}

#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformRegistry.hpp"

TEST_CASE("Data Transform: Line Angle - Parameter Factory", "[transforms][line_angle][factory]") {
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

    auto params_base = std::make_unique<LineAngleParameters>();
    REQUIRE(params_base != nullptr);

    const nlohmann::json params_json = {
        {"position", 0.75},
        {"method", "Polynomial Fit"},
        {"polynomial_order", 5},
        {"reference_x", 0.0},
        {"reference_y", 1.0}
    };

    for (auto const& [key, val] : params_json.items()) {
        factory.setParameter("Calculate Line Angle", params_base.get(), key, val, nullptr);
    }

    auto* params = dynamic_cast<LineAngleParameters*>(params_base.get());
    REQUIRE(params != nullptr);

    REQUIRE(params->position == 0.75f);
    REQUIRE(params->method == AngleCalculationMethod::PolynomialFit);
    REQUIRE(params->polynomial_order == 5);
    REQUIRE(params->reference_x == 0.0f);
    REQUIRE(params->reference_y == 1.0f);
}

TEST_CASE("Data Transform: Line Angle - load_data_from_json_config", "[transforms][line_angle][json_config]") {
    // Create DataManager and populate it with LineData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test line data in code - horizontal line
    auto test_line = std::make_shared<LineData>();
    std::vector<float> x_coords = {0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> y_coords = {1.0f, 1.0f, 1.0f, 1.0f}; // Horizontal line
    test_line->emplaceAtTime(TimeFrameIndex(100), x_coords, y_coords);
    test_line->setTimeFrame(time_frame);
    
    // Store the line data in DataManager with a known key
    dm.setData("test_line", test_line, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Angle Pipeline\",\n"
        "            \"description\": \"Test line angle calculation on line data\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Angle\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_angles\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"Direct Points\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"reference_x\": 1.0,\n"
        "                    \"reference_y\": 0.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_angle_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    // Execute the transformation pipeline using load_data_from_json_config
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    auto result_angles = dm.getData<AnalogTimeSeries>("line_angles");
    REQUIRE(result_angles != nullptr);
    
    // Verify the line angle results - horizontal line should have 0 degrees
    REQUIRE(result_angles->getAnalogTimeSeries().size() == 1);
    REQUIRE(result_angles->getTimeSeries().size() == 1);
    REQUIRE(result_angles->getTimeSeries()[0] == TimeFrameIndex(100));
    REQUIRE_THAT(result_angles->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    
    // Test another pipeline with different parameters (polynomial fit)
    const char* json_config_poly = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Angle with Polynomial Fit\",\n"
        "            \"description\": \"Test line angle calculation with polynomial fitting\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Angle\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_angles_poly\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"Polynomial Fit\",\n"
        "                    \"polynomial_order\": 2,\n"
        "                    \"reference_x\": 1.0,\n"
        "                    \"reference_y\": 0.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_poly = test_dir / "pipeline_config_poly.json";
    {
        std::ofstream json_file(json_filepath_poly);
        REQUIRE(json_file.is_open());
        json_file << json_config_poly;
        json_file.close();
    }
    
    // Execute the polynomial fit pipeline
    auto data_info_list_poly = load_data_from_json_config(&dm, json_filepath_poly.string());
    
    // Verify the polynomial fit results
    auto result_angles_poly = dm.getData<AnalogTimeSeries>("line_angles_poly");
    REQUIRE(result_angles_poly != nullptr);
    
    // For a horizontal line, polynomial fit should also give 0 degrees
    REQUIRE(result_angles_poly->getAnalogTimeSeries().size() == 1);
    REQUIRE(result_angles_poly->getTimeSeries().size() == 1);
    REQUIRE_THAT(result_angles_poly->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    
    // Test with different reference vector
    const char* json_config_ref = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Line Angle with Vertical Reference\",\n"
        "            \"description\": \"Test line angle calculation with vertical reference\",\n"
        "            \"version\": \"1.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"Calculate Line Angle\",\n"
        "                \"phase\": \"analysis\",\n"
        "                \"input_key\": \"test_line\",\n"
        "                \"output_key\": \"line_angles_ref\",\n"
        "                \"parameters\": {\n"
        "                    \"position\": 0.5,\n"
        "                    \"method\": \"Direct Points\",\n"
        "                    \"polynomial_order\": 3,\n"
        "                    \"reference_x\": 0.0,\n"
        "                    \"reference_y\": 1.0\n"
        "                }\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_ref = test_dir / "pipeline_config_ref.json";
    {
        std::ofstream json_file(json_filepath_ref);
        REQUIRE(json_file.is_open());
        json_file << json_config_ref;
        json_file.close();
    }
    
    // Execute the reference vector pipeline
    auto data_info_list_ref = load_data_from_json_config(&dm, json_filepath_ref.string());
    
    // Verify the reference vector results
    auto result_angles_ref = dm.getData<AnalogTimeSeries>("line_angles_ref");
    REQUIRE(result_angles_ref != nullptr);
    
    // With vertical reference (0,1), horizontal line should be -90 degrees
    REQUIRE(result_angles_ref->getAnalogTimeSeries().size() == 1);
    REQUIRE(result_angles_ref->getTimeSeries().size() == 1);
    REQUIRE_THAT(result_angles_ref->getAnalogTimeSeries()[0], Catch::Matchers::WithinAbs(-90.0f, 0.001f));
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
