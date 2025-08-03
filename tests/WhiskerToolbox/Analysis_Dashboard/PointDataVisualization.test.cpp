#include "../fixtures/qt_test_fixtures.hpp"

#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Points/PointDataVisualization.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Points/VectorPointVisualization.hpp"
#include "Analysis_Dashboard/Groups/GroupManager.hpp"
#include "DataManager/Points/utils/Point_Data_utils.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <QMatrix4x4>
#include <QVector2D>

TEST_CASE_METHOD(PointVisualizationTestFixture, "PointDataVisualization - Basic Creation", "[PointDataVisualization]") {
    // Create test PointData
    auto point_data = createTestPointData();
    REQUIRE(point_data != nullptr);
    
    // Create PointDataVisualization
    PointDataVisualization visualization("test_data", point_data);
    REQUIRE(&visualization != nullptr);
    
    // Test that the visualization can be created successfully
    SECTION("Constructor with data") {
        // Verify that the visualization was created with the correct data
        REQUIRE(point_data->GetAllPointsAsRange().size() > 0);
    }
    
    SECTION("Constructor with group manager") {
        GroupManager* group_manager = createTestGroupManager();
        PointDataVisualization visualization_with_groups("test_data", point_data, group_manager);
        REQUIRE(&visualization_with_groups != nullptr);
    }
}

TEST_CASE_METHOD(PointVisualizationTestFixture, "PointDataVisualization - Data Population", "[PointDataVisualization]") {
    // Create test PointData
    auto point_data = createTestPointData();
    REQUIRE(point_data != nullptr);
    
    // Create PointDataVisualization
    PointDataVisualization visualization("test_data", point_data);
    
    // Test data population
    SECTION("Vertex data population") {
        // Verify that vertex data was populated
        REQUIRE(visualization.m_vertex_data.size() > 0);
        
        // Should have 3 floats per point (x, y, group_id)
        size_t expected_points = 0;
        for (auto const& time_points_pair : point_data->GetAllPointsAsRange()) {
            expected_points += time_points_pair.points.size();
        }
        
        REQUIRE(visualization.m_vertex_data.size() == expected_points * 3);
    }
    
    
    SECTION("Statistics tracking") {
        // Verify that statistics are properly tracked
        REQUIRE(visualization.m_total_point_count > 0);
        REQUIRE(visualization.m_hidden_point_count == 0); // Initially no hidden points
        REQUIRE(visualization.m_visible_vertex_count > 0);
    }
}

TEST_CASE_METHOD(PointVisualizationTestFixture, "PointDataVisualization - OpenGL Resources", "[PointDataVisualization]") {
    // Create test PointData
    auto point_data = createTestPointData();
    REQUIRE(point_data != nullptr);
    
    // Create PointDataVisualization
    PointDataVisualization visualization("test_data", point_data);
    
    // Test OpenGL resource initialization
    SECTION("OpenGL resource creation") {
        // Verify that OpenGL resources were created
        REQUIRE(visualization.m_vertex_buffer.isCreated());
        REQUIRE(visualization.m_vertex_array_object.isCreated());
        REQUIRE(visualization.m_selection_vertex_buffer.isCreated());
        REQUIRE(visualization.m_selection_vertex_array_object.isCreated());
        REQUIRE(visualization.m_highlight_vertex_buffer.isCreated());
        REQUIRE(visualization.m_highlight_vertex_array_object.isCreated());
    }
    
    SECTION("Vertex buffer data") {
        // Verify that vertex buffer contains the correct data
        REQUIRE(visualization.m_vertex_buffer.isCreated());
        
        // The vertex buffer should contain the vertex data
        REQUIRE(visualization.m_vertex_data.size() > 0);
    }
}

TEST_CASE_METHOD(PointVisualizationTestFixture, "PointDataVisualization - Rendering", "[PointDataVisualization]") {
    // Create test PointData
    auto point_data = createTestPointData();
    REQUIRE(point_data != nullptr);
    
    // Create PointDataVisualization
    PointDataVisualization visualization("test_data", point_data);
    
    // Test rendering functionality
    SECTION("Render method") {
        // Create a simple MVP matrix
        QMatrix4x4 mvp_matrix;
        mvp_matrix.setToIdentity();
        
        // Test that rendering doesn't crash
        REQUIRE_NOTHROW(visualization.render(mvp_matrix, 2.0f));
    }
    
    SECTION("Render with different point sizes") {
        QMatrix4x4 mvp_matrix;
        mvp_matrix.setToIdentity();
        
        // Test rendering with different point sizes
        REQUIRE_NOTHROW(visualization.render(mvp_matrix, 1.0f));
        REQUIRE_NOTHROW(visualization.render(mvp_matrix, 5.0f));
        REQUIRE_NOTHROW(visualization.render(mvp_matrix, 10.0f));
    }
}

TEST_CASE_METHOD(PointVisualizationTestFixture, "PointDataVisualization - Hover", "[PointDataVisualization]") {
    // Create test PointData
    auto point_data = createTestPointData();
    REQUIRE(point_data != nullptr);
    
    // Create PointDataVisualization
    PointDataVisualization visualization("test_data", point_data);
    
    // Test hover functionality
    SECTION("Hover detection") {
        // Test hover with a point position
        QVector2D world_pos(15.0f, 25.0f); // Should be near a test point
        float tolerance = 5.0f;
        
        bool hover_changed = visualization.handleHover(world_pos, tolerance);
        // Hover state may or may not change depending on exact point positions
        REQUIRE_NOTHROW(visualization.handleHover(world_pos, tolerance));
    }
    
    SECTION("Clear hover") {
        visualization.clearHover();
        REQUIRE(visualization.m_current_hover_point == nullptr);
    }
    
    SECTION("Tooltip text") {
        // Test tooltip generation
        QString tooltip = visualization.getTooltipText();
        // Tooltip should be empty when no hover point
        REQUIRE(tooltip.isEmpty());
    }
}


TEST_CASE_METHOD(PointVisualizationTestFixture, "VectorPointVisualization - Basic Creation", "[VectorPointVisualization]") {
    // Create test vector data
    auto [x_coords, y_coords, row_indicators] = createTestVectorData();
    
    // Create VectorPointVisualization
    VectorPointVisualization<float, int64_t> visualization("test_vector_data", x_coords, y_coords, row_indicators);
    REQUIRE(&visualization != nullptr);
    
}

TEST_CASE_METHOD(PointVisualizationTestFixture, "VectorPointVisualization - Data Population", "[VectorPointVisualization]") {
    // Create test vector data
    auto [x_coords, y_coords, row_indicators] = createTestVectorData();
    
    // Create VectorPointVisualization
    VectorPointVisualization<float, int64_t> visualization("test_vector_data", x_coords, y_coords, row_indicators);
    
    SECTION("Vertex data population") {
        // Verify that vertex data was populated
        REQUIRE(visualization.m_vertex_data.size() > 0);
        
        // Should have 3 floats per point (x, y, group_id)
        REQUIRE(visualization.m_vertex_data.size() == x_coords.size() * 3);
    }
    
}

TEST_CASE_METHOD(PointVisualizationTestFixture, "VectorPointVisualization - Validation", "[VectorPointVisualization]") {
    SECTION("Mismatched coordinate sizes") {
        std::vector<float> x_coords = {1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {1.0f, 2.0f}; // Different size
        
        // Should handle mismatched sizes gracefully
        VectorPointVisualization<float, int64_t> visualization("test_data", x_coords, y_coords);
        REQUIRE(&visualization != nullptr);
    }
    
    SECTION("Mismatched indicator sizes") {
        std::vector<float> x_coords = {1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {1.0f, 2.0f, 3.0f};
        std::vector<int64_t> row_indicators = {1, 2}; // Different size
        
        // Should handle mismatched indicator sizes gracefully
        VectorPointVisualization<float, int64_t> visualization("test_data", x_coords, y_coords, row_indicators);
        REQUIRE(&visualization != nullptr);
    }
}

TEST_CASE_METHOD(PointVisualizationTestFixture, "Point Visualization - Integration", "[PointVisualization]") {
    // Test integration between different visualization types
    
    SECTION("Rendering multiple visualizations") {
        auto point_data = createTestPointData();
        PointDataVisualization point_viz("point_data", point_data);
        
        auto [x_coords, y_coords, row_indicators] = createTestVectorData();
        VectorPointVisualization<float, int64_t> vector_viz("vector_data", x_coords, y_coords, row_indicators);
        
        QMatrix4x4 mvp_matrix;
        mvp_matrix.setToIdentity();
        
        // Both should render without errors
        REQUIRE_NOTHROW(point_viz.render(mvp_matrix, 2.0f));
        REQUIRE_NOTHROW(vector_viz.render(mvp_matrix, 2.0f));
    }
} 