#include "PointDataVisualization.hpp"

#include "GroupManagementWidget/GroupManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Points/utils/Point_Data_utils.hpp"

#include <QOpenGLShaderProgram>

PointDataVisualization::PointDataVisualization(QString const & data_key,
                                               std::shared_ptr<PointData> const & point_data,
                                               GroupManager * group_manager)
    : GenericPointVisualization<float, EntityId>(data_key, group_manager),
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
    m_spatial_index = std::make_unique<QuadTree<EntityId>>(bounds);
    m_vertex_data.reserve(m_point_data->GetAllPointsAsRange().size() * 3);// Reserve space for x, y, group_id

    // Collect EntityIds for each point to enable proper grouping
    std::vector<EntityId> entity_ids;
    // Pre-reserve approximate; we will push for each point below

    for (auto const & time_points_pair: m_point_data->GetAllPointsAsRange()) {
        // Get EntityIds for this time frame
        auto const & frame_entity_ids = m_point_data->getEntityIdsAtTime(time_points_pair.time);
        
        for (size_t i = 0; i < time_points_pair.points.size(); ++i) {
            auto const & point = time_points_pair.points[i];
            
            // Map i to entity id for this time
            EntityId eid = 0;
            if (i < frame_entity_ids.size()) {
                eid = frame_entity_ids[i];
            }

            // Store original coordinates in QuadTree with EntityId as data
            m_spatial_index->insert(point.x, point.y, eid);

            // Store coordinates and group_id in vertex data for OpenGL rendering
            m_vertex_data.push_back(point.x);
            m_vertex_data.push_back(point.y);
            m_vertex_data.push_back(0.0f);// group_id = 0 (ungrouped) initially
            
            // Store the corresponding EntityId (or 0 if unavailable)
            entity_ids.push_back(eid);
        }
    }

    // Initialize visibility statistics
    m_total_point_count = m_vertex_data.size() / 3;// 3 components per point now
    m_hidden_point_count = 0;
    m_visible_vertex_count = m_vertex_data.size();
    
    // Set up EntityIds for grouping support
    qDebug() << "PointDataVisualization: Setting up" << entity_ids.size() << "EntityIds for grouping";
    setPerPointEntityIds(std::move(entity_ids));

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
