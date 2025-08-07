#include "ScatterPlotVisualization.hpp"

#include "Groups/GroupManager.hpp"

#include <QDebug>

ScatterPlotVisualization::ScatterPlotVisualization(
    QString const & data_key,
    std::vector<float> const & x_coords,
    std::vector<float> const & y_coords,
    GroupManager * group_manager,
    bool defer_opengl_init)
    : GenericPointVisualization<float, size_t>(data_key, group_manager, defer_opengl_init),
      m_x_label("X"),
      m_y_label("Y"),
      m_x_data(x_coords),
      m_y_data(y_coords) {
    
    // Validate input data
    if (m_x_data.size() != m_y_data.size()) {
        qDebug() << "ScatterPlotVisualization: X and Y coordinate vectors must have the same size";
        return;
    }
    
    // Populate data from vectors
    populateData();
    
    qDebug() << "ScatterPlotVisualization: Created with" 
             << x_coords.size() << "points";
}

void ScatterPlotVisualization::updateData(std::vector<float> const & x_coords, 
                                         std::vector<float> const & y_coords) {
    if (x_coords.size() != y_coords.size()) {
        qDebug() << "ScatterPlotVisualization::updateData: X and Y coordinate vectors must have the same size";
        return;
    }

    // Store copies for tooltip generation
    m_x_data = x_coords;
    m_y_data = y_coords;

    // Repopulate the visualization data
    populateData();
    
    qDebug() << "ScatterPlotVisualization: Updated data with" 
             << x_coords.size() << "points";
}

void ScatterPlotVisualization::populateData() {
    if (m_x_data.empty()) {
        qDebug() << "ScatterPlotVisualization: No coordinate data provided";
        return;
    }

    qDebug() << "ScatterPlotVisualization::populateData: Starting with" << m_x_data.size() << "points";

    // Calculate bounds for QuadTree initialization
    BoundingBox bounds = getDataBounds();
    qDebug() << "ScatterPlotVisualization::populateData: Data bounds:" 
             << bounds.min_x << "," << bounds.min_y << "to" << bounds.max_x << "," << bounds.max_y;

    // Update the spatial index with proper bounds
    this->m_spatial_index = std::make_unique<QuadTree<size_t>>(bounds);
    this->m_vertex_data.reserve(m_x_data.size() * 3);// Reserve space for x, y, group_id

    for (size_t i = 0; i < m_x_data.size(); ++i) {
        float x = m_x_data[i];
        float y = m_y_data[i];
        
        // Use vector index as row indicator
        size_t row_indicator = i;

        // Store original coordinates in QuadTree for spatial queries
        this->m_spatial_index->insert(x, y, row_indicator);

        // Store original world coordinates in vertex data for OpenGL rendering
        // The projection matrix will handle the coordinate transformation
        this->m_vertex_data.push_back(x);
        this->m_vertex_data.push_back(y);
        this->m_vertex_data.push_back(0.0f);// group_id = 0 (ungrouped) initially
    }

    // Initialize visibility statistics
    this->m_total_point_count = this->m_vertex_data.size() / 3;// 3 components per point
    this->m_hidden_point_count = 0;
    this->m_visible_vertex_count = this->m_vertex_data.size();

    qDebug() << "ScatterPlotVisualization::populateData: Created" << this->m_total_point_count 
             << "points with" << this->m_vertex_data.size() << "vertex components";

    // Update the OpenGL vertex buffer with the populated data
    if (this->m_vertex_buffer.isCreated()) {
        this->m_vertex_buffer.bind();
        this->m_vertex_buffer.allocate(this->m_vertex_data.data(), 
                                       static_cast<int>(this->m_vertex_data.size() * sizeof(float)));
        this->m_vertex_buffer.release();
        qDebug() << "ScatterPlotVisualization::populateData: Updated OpenGL vertex buffer";
    } else {
        qDebug() << "ScatterPlotVisualization::populateData: Vertex buffer not created yet";
    }

    qDebug() << "ScatterPlotVisualization: Populated data with" << this->m_total_point_count << "points";
}

BoundingBox ScatterPlotVisualization::getDataBounds() const {
    if (m_x_data.empty()) {
        return BoundingBox(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Find min/max coordinates
    float min_x = m_x_data[0];
    float max_x = m_x_data[0];
    float min_y = m_y_data[0];
    float max_y = m_y_data[0];

    for (size_t i = 1; i < m_x_data.size(); ++i) {
        min_x = std::min(min_x, m_x_data[i]);
        max_x = std::max(max_x, m_x_data[i]);
        min_y = std::min(min_y, m_y_data[i]);
        max_y = std::max(max_y, m_y_data[i]);
    }

    return BoundingBox(min_x, min_y, max_x, max_y);
}

void ScatterPlotVisualization::setAxisLabels(QString const & x_label, QString const & y_label) {
    m_x_label = x_label;
    m_y_label = y_label;
}

QString ScatterPlotVisualization::getTooltipText(size_t row_indicator) {
    // row_indicator should be the index in our data vectors
    if (row_indicator >= m_x_data.size() || row_indicator >= m_y_data.size()) {
        return QString("Point %1\nInvalid data").arg(row_indicator);
    }

    float x_value = m_x_data[row_indicator];
    float y_value = m_y_data[row_indicator];

    return QString("Point %1\n%2: %3\n%4: %5")
        .arg(row_indicator)
        .arg(m_x_label)
        .arg(x_value, 0, 'f', 3)
        .arg(m_y_label)
        .arg(y_value, 0, 'f', 3);
}
