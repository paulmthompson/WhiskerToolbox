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

TEST_CASE("ViewMatrix", "[CorePlotting][SeriesMatrices]") {
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

TEST_CASE("ProjectionMatrix", "[CorePlotting][SeriesMatrices]") {
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

// ============================================================================
// LayoutTransform-based Model Matrix Tests
// ============================================================================

TEST_CASE("createModelMatrix from LayoutTransform", "[CorePlotting][SeriesMatrices]") {
    SECTION("Identity transform gives identity matrix") {
        LayoutTransform identity{0.0f, 1.0f};
        
        glm::mat4 M = createModelMatrix(identity);
        
        REQUIRE(M == glm::mat4(1.0f));
    }
    
    SECTION("Scale-only transform") {
        LayoutTransform scale_only{0.0f, 2.0f};  // gain=2, offset=0
        
        glm::mat4 M = createModelMatrix(scale_only);
        
        REQUIRE(M[1][1] == 2.0f);  // Y scale
        REQUIRE(M[3][1] == 0.0f);  // No Y translation
    }
    
    SECTION("Offset-only transform") {
        LayoutTransform offset_only{5.0f, 1.0f};  // offset=5, gain=1
        
        glm::mat4 M = createModelMatrix(offset_only);
        
        REQUIRE(M[1][1] == 1.0f);  // No Y scale change
        REQUIRE(M[3][1] == 5.0f);  // Y translation = 5
    }
    
    SECTION("Combined scale and offset") {
        LayoutTransform combined{3.0f, 0.5f};  // offset=3, gain=0.5
        
        glm::mat4 M = createModelMatrix(combined);
        
        REQUIRE(M[1][1] == 0.5f);  // Y scale = 0.5
        REQUIRE(M[3][1] == 3.0f);  // Y translation = 3
    }
}

TEST_CASE("createViewMatrix", "[CorePlotting][SeriesMatrices]") {
    SECTION("No pan gives identity") {
        glm::mat4 V = createViewMatrix(0.0f);
        REQUIRE(V == glm::mat4(1.0f));
    }
    
    SECTION("Pan offset creates translation") {
        glm::mat4 V = createViewMatrix(25.0f);
        REQUIRE(V[3][1] == 25.0f);
        REQUIRE(V[3][0] == 0.0f);
    }
}

TEST_CASE("createProjectionMatrix", "[CorePlotting][SeriesMatrices]") {
    SECTION("Valid parameters create ortho matrix") {
        TimeFrameIndex start(0);
        TimeFrameIndex end(1000);
        
        glm::mat4 P = createProjectionMatrix(start, end, -1.0f, 1.0f);
        
        // Check matrix is valid
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(P[i][j]));
            }
        }
    }
}
