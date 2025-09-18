#include "EventPointVisualization.hpp"

#include "GroupManagementWidget/GroupManager.hpp"

#include <QDebug>
#include <limits>

EventPointVisualization::EventPointVisualization(QString const & data_key,
                                                 std::vector<std::vector<float>> const & event_data,
                                                 GroupManager * group_manager,
                                                 bool defer_opengl_init)
    : GenericPointVisualization<size_t>(data_key, group_manager, defer_opengl_init),
      m_event_data(event_data) {
    
    // Populate data from event vectors
    populateData();
}

void EventPointVisualization::populateData() {
    if (m_event_data.empty()) {
        qDebug() << "EventPointVisualization: No event data provided";
        return;
    }

    // Calculate total number of events for pre-allocation
    size_t total_events = 0;
    
    for (auto const & trial: m_event_data) {
        total_events += trial.size();
    }

    if (total_events == 0) {
        qDebug() << "EventPointVisualization: No events in data";
        return;
    }

    // Calculate bounds for QuadTree initialization
    BoundingBox bounds = getDataBounds();

    // Update the spatial index with proper bounds
    this->m_spatial_index = std::make_unique<QuadTree<size_t>>(bounds);
    this->m_vertex_data.reserve(total_events * 3);// Reserve space for x, y, group_id
    m_event_mappings.reserve(total_events);

    // Convert trial index to y-coordinate (normalized to [-1, 1])
    float y_scale = 2.0f / static_cast<float>(m_event_data.size());
    size_t global_event_index = 0;

    for (size_t trial_index = 0; trial_index < m_event_data.size(); ++trial_index) {
        float y = -1.0f + (static_cast<float>(trial_index) + 0.5f) * y_scale;

        for (size_t event_index = 0; event_index < m_event_data[trial_index].size(); ++event_index) {
            float event_time = m_event_data[trial_index][event_index];

            // Store coordinates in QuadTree with global event index as identifier
            this->m_spatial_index->insert(event_time, y, global_event_index);

            // Store coordinates and group_id in vertex data for OpenGL rendering
            this->m_vertex_data.push_back(event_time);
            this->m_vertex_data.push_back(y);
            this->m_vertex_data.push_back(0.0f);// group_id = 0 (ungrouped) initially

            // Store mapping information
            m_event_mappings.push_back({
                trial_index,
                event_index,
                event_time,
                y
            });

            global_event_index++;
        }
    }

    // Initialize visibility statistics
    this->m_total_point_count = this->m_vertex_data.size() / 3;// 3 components per point
    this->m_hidden_point_count = 0;
    this->m_visible_vertex_count = this->m_vertex_data.size();

    // Update the OpenGL vertex buffer with the populated data
    if (this->m_vertex_buffer.isCreated()) {
        this->m_vertex_buffer.bind();
        this->m_vertex_buffer.allocate(this->m_vertex_data.data(), 
                                       static_cast<int>(this->m_vertex_data.size() * sizeof(float)));
        this->m_vertex_buffer.release();
    }

    qDebug() << "EventPointVisualization: Populated data with" << this->m_total_point_count 
             << "events from" << m_event_data.size() << "trials";
}

BoundingBox EventPointVisualization::getDataBounds() const {
    if (m_event_data.empty()) {
        return BoundingBox(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Find min/max event times across all trials
    float min_time = std::numeric_limits<float>::max();
    float max_time = std::numeric_limits<float>::lowest();
    bool has_events = false;

    for (auto const & trial: m_event_data) {
        for (float event_time: trial) {
            min_time = std::min(min_time, event_time);
            max_time = std::max(max_time, event_time);
            has_events = true;
        }
    }

    if (!has_events) {
        return BoundingBox(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Y bounds are always [-1, 1] for normalized trial coordinates
    return BoundingBox(min_time, -1.0f, max_time, 1.0f);
}

QString EventPointVisualization::getEventTooltipText() const {
    if (!this->m_current_hover_point) {
        return QString();
    }

    size_t point_index = this->m_current_hover_point->data;
    if (point_index >= m_event_mappings.size()) {
        return QString("Invalid event");
    }

    const EventMapping& mapping = m_event_mappings[point_index];
    
    return QString("Trial %1, Event %2\nTime: %3 ms")
        .arg(mapping.trial_index + 1)  // 1-based for display
        .arg(mapping.event_index_in_trial + 1)  // 1-based for display
        .arg(mapping.event_time, 0, 'f', 1);
}

int EventPointVisualization::getTrialIndex(size_t point_index) const {
    if (point_index >= m_event_mappings.size()) {
        return -1;
    }
    return static_cast<int>(m_event_mappings[point_index].trial_index);
}

int EventPointVisualization::getEventIndexInTrial(size_t point_index) const {
    if (point_index >= m_event_mappings.size()) {
        return -1;
    }
    return static_cast<int>(m_event_mappings[point_index].event_index_in_trial);
}

float EventPointVisualization::getEventTime(size_t point_index) const {
    if (point_index >= m_event_mappings.size()) {
        return 0.0f;
    }
    return m_event_mappings[point_index].event_time;
}
