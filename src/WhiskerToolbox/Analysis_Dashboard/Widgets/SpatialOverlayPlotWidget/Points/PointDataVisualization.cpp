#include "PointDataVisualization.hpp"

#include "Analysis_Dashboard/Groups/GroupManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Points/utils/Point_Data_utils.hpp"

#include <QOpenGLShaderProgram>

PointDataVisualization::PointDataVisualization(QString const & data_key,
                                               std::shared_ptr<PointData> const & point_data,
                                               GroupManager * group_manager)
    : GenericPointVisualization<float, int64_t>(data_key, group_manager),
      m_point_data(point_data) {
    
    // Populate data from PointData
    populateData();
}

void PointDataVisualization::populateData() {
    if (!m_point_data) {
        qDebug() << "PointDataVisualization: No PointData provided";
        return;
    }

    // Calculate bounds for QuadTree initialization
    BoundingBox bounds = calculateBoundsForPointData(m_point_data.get());

    // Update the spatial index with proper bounds
    m_spatial_index = std::make_unique<QuadTree<int64_t>>(bounds);
    m_vertex_data.reserve(m_point_data->GetAllPointsAsRange().size() * 3);// Reserve space for x, y, group_id

    for (auto const & time_points_pair: m_point_data->GetAllPointsAsRange()) {
        for (auto const & point: time_points_pair.points) {
            // Store original coordinates in QuadTree (preserve data structure)
            m_spatial_index->insert(point.x, point.y, time_points_pair.time.getValue());

            // Store coordinates and group_id in vertex data for OpenGL rendering
            m_vertex_data.push_back(point.x);
            m_vertex_data.push_back(point.y);
            m_vertex_data.push_back(0.0f);// group_id = 0 (ungrouped) initially
        }
    }

    // Initialize visibility statistics
    m_total_point_count = m_vertex_data.size() / 3;// 3 components per point now
    m_hidden_point_count = 0;
    m_visible_vertex_count = m_vertex_data.size();

    // Update the OpenGL vertex buffer with the populated data
    if (m_vertex_buffer.isCreated()) {
        m_vertex_buffer.bind();
        m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));
        m_vertex_buffer.release();
    }

    qDebug() << "PointDataVisualization: Populated data with" << m_total_point_count << "points";
}

BoundingBox PointDataVisualization::getDataBounds() const {
    if (!m_point_data) {
        return BoundingBox(0.0f, 0.0f, 1.0f, 1.0f);
    }
    return calculateBoundsForPointData(m_point_data.get());
}
