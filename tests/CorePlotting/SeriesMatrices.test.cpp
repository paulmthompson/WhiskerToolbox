#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"
#include "TimeFrame/TimeFrame.hpp"

using namespace CorePlotting;

TEST_CASE("Utility: validateOrthoParams", "[CorePlotting][SeriesMatrices]") {
    SECTION("Valid parameters unchanged") {
        float left = 0.0f, right = 100.0f, bottom = -10.0f, top = 10.0f;
        bool valid = validateOrthoParams(left, right, bottom, top, "Test");
        
        REQUIRE(valid == true);
        REQUIRE(left == 0.0f);
        REQUIRE(right == 100.0f);
        REQUIRE(bottom == -10.0f);
        REQUIRE(top == 10.0f);
    }
    
    SECTION("Inverted X range is corrected") {
        float left = 100.0f, right = 0.0f, bottom = -10.0f, top = 10.0f;
        bool valid = validateOrthoParams(left, right, bottom, top, "Test");
        
        REQUIRE(valid == false);
        // The function should have attempted to fix it by setting both near the center
        REQUIRE(left <= right);  // At minimum they should be equal or properly ordered
        REQUIRE(std::abs(left - 50.0f) < 1.0f);  // Should be near original midpoint
        REQUIRE(std::abs(right - 50.0f) < 1.0f);
    }
    
    SECTION("Inverted Y range is corrected") {
        float left = 0.0f, right = 100.0f, bottom = 10.0f, top = -10.0f;
        bool valid = validateOrthoParams(left, right, bottom, top, "Test");
        
        REQUIRE(valid == false);
        REQUIRE(bottom < top);  // Now corrected
    }
    
    SECTION("NaN values are replaced") {
        float left = std::numeric_limits<float>::quiet_NaN();
        float right = 100.0f, bottom = -10.0f, top = 10.0f;
        bool valid = validateOrthoParams(left, right, bottom, top, "Test");
        
        REQUIRE(valid == false);
        REQUIRE(std::isfinite(left));
    }
    
    SECTION("Too-small range is expanded") {
        float left = 50.0f, right = 50.0f + 5e-7f, bottom = -10.0f, top = 10.0f;  // Smaller than 1e-6
        float original_center = (left + right) / 2.0f;
        bool valid = validateOrthoParams(left, right, bottom, top, "Test");
        
        REQUIRE(valid == false);  // Should be invalid since range < 1e-6
        // Function should attempt to expand the range
        REQUIRE(left <= right);
        // Center should be preserved
        REQUIRE(std::abs((left + right) / 2.0f - original_center) < 0.001f);
    }
}

TEST_CASE("Utility: validateMatrix", "[CorePlotting][SeriesMatrices]") {
    SECTION("Valid matrix unchanged") {
        glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        glm::mat4 result = validateMatrix(mat, "Test");
        
        REQUIRE(result == mat);
    }
    
    SECTION("Matrix with NaN returns identity") {
        glm::mat4 mat(1.0f);
        mat[0][0] = std::numeric_limits<float>::quiet_NaN();
        glm::mat4 result = validateMatrix(mat, "Test");
        
        REQUIRE(result == glm::mat4(1.0f));
    }
}

TEST_CASE("AnalogModelMatrix basic functionality", "[CorePlotting][SeriesMatrices]") {
    SECTION("Identity case - no scaling, no offset") {
        AnalogSeriesMatrixParams params;
        params.allocated_y_center = 0.0f;
        params.allocated_height = 1.0f;
        params.intrinsic_scale = 1.0f;
        params.user_scale_factor = 1.0f;
        params.global_zoom = 1.0f;
        params.global_vertical_scale = 1.0f;
        params.data_mean = 0.0f;
        params.std_dev = 1.0f;
        params.user_vertical_offset = 0.0f;
        
        glm::mat4 M = getAnalogModelMatrix(params);
        
        // With std_dev=1, intrinsic_scale=1/(3*1)=1/3
        // total_y_scale = (1/3) * 1 * 1 * 1 * 1 = 1/3
        // height_scale = 1.0 * 0.8 = 0.8
        // final_y_scale = (1/3) * 0.8 = 0.8/3 ≈ 0.2667
        float expected_scale = (1.0f / 3.0f) * 1.0f * 1.0f * 1.0f * 1.0f * 0.8f;
        
        REQUIRE_THAT(M[1][1], Catch::Matchers::WithinRel(expected_scale, 0.001f));
        REQUIRE_THAT(M[3][1], Catch::Matchers::WithinAbs(0.0f, 0.001f)); // y_offset with mean=0
    }
    
    SECTION("Data mean shifts Y position") {
        AnalogSeriesMatrixParams params;
        params.allocated_y_center = 100.0f;
        params.allocated_height = 50.0f;
        params.data_mean = 10.0f;  // Non-zero mean
        params.std_dev = 2.0f;
        
        glm::mat4 M = getAnalogModelMatrix(params);
        
        // Should center data around allocated_y_center
        // Check that matrix is valid
        REQUIRE(std::isfinite(M[1][1]));
        REQUIRE(std::isfinite(M[3][1]));
        
        // The Y translation should incorporate the mean offset
        REQUIRE(M[3][1] != 100.0f); // Should differ due to mean correction
    }
    
    SECTION("User vertical offset applied") {
        AnalogSeriesMatrixParams params_no_offset;
        params_no_offset.allocated_y_center = 0.0f;
        params_no_offset.allocated_height = 1.0f;
        params_no_offset.data_mean = 0.0f;
        params_no_offset.std_dev = 1.0f;
        params_no_offset.intrinsic_scale = 1.0f;
        params_no_offset.user_scale_factor = 1.0f;
        params_no_offset.global_zoom = 1.0f;
        params_no_offset.global_vertical_scale = 1.0f;
        params_no_offset.user_vertical_offset = 0.0f;
        
        AnalogSeriesMatrixParams params_with_offset = params_no_offset;
        params_with_offset.user_vertical_offset = 25.0f;
        
        glm::mat4 M_no_offset = getAnalogModelMatrix(params_no_offset);
        glm::mat4 M_with_offset = getAnalogModelMatrix(params_with_offset);
        
        // Debug output
        INFO("M_no_offset[3][1] = " << M_no_offset[3][1]);
        INFO("M_with_offset[3][1] = " << M_with_offset[3][1]);
        INFO("M_with_offset[1][1] (scale) = " << M_with_offset[1][1]);
        
        // The difference should reflect the user offset
        // Note: glm::translate applies the translation *after* the Model transformation,
        // which means the translation is affected by the Y scaling in the model matrix
        float y_scale = M_with_offset[1][1];
        float translation_diff = M_with_offset[3][1] - M_no_offset[3][1];
        
        // The actual difference will be user_vertical_offset, not scaled
        REQUIRE(std::abs(translation_diff - 25.0f) < 1.0f);
    }
}

TEST_CASE("AnalogViewMatrix", "[CorePlotting][SeriesMatrices]") {
    SECTION("No pan offset gives identity") {
        ViewProjectionParams params;
        params.vertical_pan_offset = 0.0f;
        
        glm::mat4 V = getAnalogViewMatrix(params);
        
        REQUIRE(V == glm::mat4(1.0f));
    }
    
    SECTION("Pan offset creates translation") {
        ViewProjectionParams params;
        params.vertical_pan_offset = 50.0f;
        
        glm::mat4 V = getAnalogViewMatrix(params);
        
        REQUIRE(V[3][1] == 50.0f);  // Y translation
        REQUIRE(V[3][0] == 0.0f);   // No X translation
    }
}

TEST_CASE("AnalogProjectionMatrix", "[CorePlotting][SeriesMatrices]") {
    SECTION("Valid time range creates ortho matrix") {
        TimeFrameIndex start(0);
        TimeFrameIndex end(1000);
        float y_min = -10.0f;
        float y_max = 10.0f;
        
        glm::mat4 P = getAnalogProjectionMatrix(start, end, y_min, y_max);
        
        // Should create valid orthographic projection
        REQUIRE(std::isfinite(P[0][0]));
        REQUIRE(std::isfinite(P[1][1]));
        REQUIRE(std::isfinite(P[3][0]));
        REQUIRE(std::isfinite(P[3][1]));
    }
    
    SECTION("Invalid parameters corrected gracefully") {
        TimeFrameIndex start(100);
        TimeFrameIndex end(100);  // Same as start (invalid)
        float y_min = 5.0f;
        float y_max = -5.0f;  // Inverted (invalid)
        
        glm::mat4 P = getAnalogProjectionMatrix(start, end, y_min, y_max);
        
        // Should still produce valid matrix (with corrections)
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(P[i][j]));
            }
        }
    }
}

TEST_CASE("EventModelMatrix plotting modes", "[CorePlotting][SeriesMatrices]") {
    SECTION("FullCanvas mode uses viewport bounds") {
        EventSeriesMatrixParams params;
        params.plotting_mode = EventSeriesMatrixParams::PlottingMode::FullCanvas;
        params.viewport_y_min = -100.0f;
        params.viewport_y_max = 100.0f;
        params.margin_factor = 1.0f;
        params.global_vertical_scale = 1.0f;
        
        glm::mat4 M = getEventModelMatrix(params);
        
        // Should scale to full viewport height
        float expected_height = 200.0f * 1.0f;  // (max - min) * margin
        REQUIRE_THAT(M[1][1], Catch::Matchers::WithinRel(expected_height * 0.5f, 0.01f));
        
        // Should center at viewport center
        REQUIRE_THAT(M[3][1], Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Stacked mode uses allocated space") {
        EventSeriesMatrixParams params;
        params.plotting_mode = EventSeriesMatrixParams::PlottingMode::Stacked;
        params.allocated_y_center = 50.0f;
        params.allocated_height = 20.0f;
        params.event_height = 0.0f;  // Use allocated height
        params.margin_factor = 0.8f;
        params.global_vertical_scale = 1.0f;
        
        glm::mat4 M = getEventModelMatrix(params);
        
        // Should scale to allocated height
        float expected_scale = 20.0f * 0.8f * 0.5f;
        REQUIRE_THAT(M[1][1], Catch::Matchers::WithinRel(expected_scale, 0.01f));
        
        // Should center at allocated center
        REQUIRE(M[3][1] == 50.0f);
    }
    
    SECTION("Stacked mode with explicit event height") {
        EventSeriesMatrixParams params;
        params.plotting_mode = EventSeriesMatrixParams::PlottingMode::Stacked;
        params.allocated_y_center = 50.0f;
        params.allocated_height = 20.0f;
        params.event_height = 10.0f;  // Smaller than allocated
        params.margin_factor = 1.0f;
        params.global_vertical_scale = 1.0f;
        
        glm::mat4 M = getEventModelMatrix(params);
        
        // Should use event_height (smaller of the two)
        float expected_scale = 10.0f * 1.0f * 0.5f;
        REQUIRE_THAT(M[1][1], Catch::Matchers::WithinRel(expected_scale, 0.01f));
    }
}

TEST_CASE("EventViewMatrix panning behavior", "[CorePlotting][SeriesMatrices]") {
    SECTION("FullCanvas mode ignores panning") {
        EventSeriesMatrixParams params;
        params.plotting_mode = EventSeriesMatrixParams::PlottingMode::FullCanvas;
        
        ViewProjectionParams view_params;
        view_params.vertical_pan_offset = 100.0f;
        
        glm::mat4 V = getEventViewMatrix(params, view_params);
        
        REQUIRE(V == glm::mat4(1.0f));  // No panning applied
    }
    
    SECTION("Stacked mode applies panning") {
        EventSeriesMatrixParams params;
        params.plotting_mode = EventSeriesMatrixParams::PlottingMode::Stacked;
        
        ViewProjectionParams view_params;
        view_params.vertical_pan_offset = 100.0f;
        
        glm::mat4 V = getEventViewMatrix(params, view_params);
        
        REQUIRE(V[3][1] == 100.0f);  // Panning applied
    }
}

TEST_CASE("IntervalModelMatrix", "[CorePlotting][SeriesMatrices]") {
    SECTION("Full canvas mode scales to allocated height") {
        IntervalSeriesMatrixParams params;
        params.allocated_y_center = 25.0f;
        params.allocated_height = 50.0f;
        params.margin_factor = 1.0f;
        params.global_zoom = 1.0f;
        params.global_vertical_scale = 1.0f;
        params.extend_full_canvas = true;
        
        glm::mat4 M = getIntervalModelMatrix(params);
        
        // Should have translation to center
        REQUIRE(M[3][1] != 0.0f);
    }
}

TEST_CASE("IntervalViewMatrix", "[CorePlotting][SeriesMatrices]") {
    SECTION("Always returns identity (viewport-pinned)") {
        ViewProjectionParams params;
        params.vertical_pan_offset = 100.0f;  // Should be ignored
        
        glm::mat4 V = getIntervalViewMatrix(params);
        
        REQUIRE(V == glm::mat4(1.0f));
    }
}

TEST_CASE("MVP matrix composition", "[CorePlotting][SeriesMatrices]") {
    SECTION("Analog series full MVP chain") {
        // Model: Position series at y=50 with height 20
        AnalogSeriesMatrixParams model_params;
        model_params.allocated_y_center = 50.0f;
        model_params.allocated_height = 20.0f;
        model_params.data_mean = 0.0f;
        model_params.std_dev = 1.0f;
        
        // View: Pan down by 10
        ViewProjectionParams view_params;
        view_params.vertical_pan_offset = -10.0f;
        
        // Projection: Time range [0, 1000], Y range [-100, 100]
        TimeFrameIndex start(0);
        TimeFrameIndex end(1000);
        
        glm::mat4 M = getAnalogModelMatrix(model_params);
        glm::mat4 V = getAnalogViewMatrix(view_params);
        glm::mat4 P = getAnalogProjectionMatrix(start, end, -100.0f, 100.0f);
        
        // Compose MVP
        glm::mat4 MVP = P * V * M;
        
        // Check all elements are finite
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(MVP[i][j]));
            }
        }
    }
}
