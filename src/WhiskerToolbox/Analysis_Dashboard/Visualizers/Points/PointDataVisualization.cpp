#include "PointDataVisualization.hpp"

#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Points/utils/Point_Data_utils.hpp"
#include "GroupManagementWidget/GroupManager.hpp"

#include <QOpenGLShaderProgram>

PointDataVisualization::PointDataVisualization(QString const & data_key,
                                               std::shared_ptr<PointData> const & point_data,
                                               GroupManager * group_manager)
    : GenericPointVisualization<EntityId>(data_key, group_manager),
      m_point_data(point_data) {

    populateData();
}

void PointDataVisualization::populateData() {
    if (!m_point_data) {
        qDebug() << "PointDataVisualization: No PointData provided";
        return;
    }

    BoundingBox const bounds = calculateBoundsForPointData(m_point_data.get());

    m_spatial_index = std::make_unique<QuadTree<EntityId>>(bounds);
    m_vertex_data.reserve(m_point_data->GetAllPointsAsRange().size() * 3); // Reserve space for x, y, group_id

    std::vector<EntityId> entity_ids;

    for (auto const & time_points_pair: m_point_data->GetAllPointsAsRange()) {

        auto const & frame_entity_ids = m_point_data->getEntityIdsAtTime(time_points_pair.time);

        for (size_t i = 0; i < time_points_pair.points.size(); ++i) {
            auto const & point = time_points_pair.points[i];

            EntityId eid = EntityId(0);
            if (i < frame_entity_ids.size()) {
                eid = frame_entity_ids[i];
            }

            m_spatial_index->insert(point.x, point.y, eid);

            m_vertex_data.push_back(point.x);
            m_vertex_data.push_back(point.y);
            m_vertex_data.push_back(0.0f);

            entity_ids.push_back(eid);
        }
    }

    m_total_point_count = m_vertex_data.size() / 3;
    m_hidden_point_count = 0;
    m_visible_vertex_count = m_vertex_data.size();

    qDebug() << "PointDataVisualization: Setting up" << entity_ids.size() << "EntityIds for grouping";
    setPerPointEntityIds(std::move(entity_ids));

    if (m_vertex_buffer.isCreated()) {
        m_vertex_buffer.bind();
        m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));
        m_vertex_buffer.release();
    }

    qDebug() << "PointDataVisualization: Populated data with" << m_total_point_count << "points";
}

BoundingBox PointDataVisualization::getDataBounds() const {
    if (!m_point_data) {
        return {0.0f, 0.0f, 1.0f, 1.0f};
    }
    return calculateBoundsForPointData(m_point_data.get());
}
