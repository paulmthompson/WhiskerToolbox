#ifndef VECTORPOINTVISUALIZATION_HPP
#define VECTORPOINTVISUALIZATION_HPP

#include "GenericPointVisualization.hpp"

#include <vector>

/**
 * @brief Example implementation of point visualization using vector data
 * 
 * This class demonstrates how to use the GenericPointVisualization
 * with simple vector data instead of PointData objects.
 * 
 * @tparam CoordType The coordinate type (float or double)
 * @tparam RowIndicatorType The type used for row indicators (e.g., int, size_t)
 */
template<typename CoordType, typename RowIndicatorType>
struct VectorPointVisualization : public GenericPointVisualization<CoordType, RowIndicatorType> {
public:
    /**
     * @brief Constructor for vector-based point visualization
     * @param data_key The key identifier for this visualization
     * @param x_coords Vector of X coordinates
     * @param y_coords Vector of Y coordinates
     * @param row_indicators Vector of row indicators (optional, uses indices if not provided)
     * @param group_manager Optional group manager for color coding
     */
    VectorPointVisualization(QString const & data_key,
                             std::vector<CoordType> const & x_coords,
                             std::vector<CoordType> const & y_coords,
                             std::vector<RowIndicatorType> const & row_indicators = {},
                             GroupManager * group_manager = nullptr);

    /**
     * @brief Destructor
     */
    ~VectorPointVisualization() override = default;

protected:
    /**
     * @brief Populate data from vector inputs
     * @pre The coordinate vectors must be the same size
     * @post The spatial index and vertex data will be populated
     */
    void populateData() override;

    /**
     * @brief Get the bounding box for the vector data
     * @return BoundingBox containing all data points
     */
    BoundingBox getDataBounds() const override;

private:
    std::vector<CoordType> m_x_coords;
    std::vector<CoordType> m_y_coords;
    std::vector<RowIndicatorType> m_row_indicators;
    bool m_use_provided_indicators;
};

// Template implementation
template<typename CoordType, typename RowIndicatorType>
VectorPointVisualization<CoordType, RowIndicatorType>::VectorPointVisualization(
    QString const & data_key,
    std::vector<CoordType> const & x_coords,
    std::vector<CoordType> const & y_coords,
    std::vector<RowIndicatorType> const & row_indicators,
    GroupManager * group_manager)
    : GenericPointVisualization<CoordType, RowIndicatorType>(data_key, group_manager),
      m_x_coords(x_coords),
      m_y_coords(y_coords),
      m_row_indicators(row_indicators),
      m_use_provided_indicators(!row_indicators.empty()) {
    
    // Validate input data
    if (m_x_coords.size() != m_y_coords.size()) {
        qDebug() << "VectorPointVisualization: X and Y coordinate vectors must have the same size";
        return;
    }
    
    if (m_use_provided_indicators && m_row_indicators.size() != m_x_coords.size()) {
        qDebug() << "VectorPointVisualization: Row indicators vector must have the same size as coordinate vectors";
        return;
    }
    
    // Populate data from vectors
    populateData();
}

template<typename CoordType, typename RowIndicatorType>
void VectorPointVisualization<CoordType, RowIndicatorType>::populateData() {
    if (m_x_coords.empty()) {
        qDebug() << "VectorPointVisualization: No coordinate data provided";
        return;
    }

    // Calculate bounds for QuadTree initialization
    BoundingBox bounds = getDataBounds();

    // Update the spatial index with proper bounds
    this->m_spatial_index = std::make_unique<QuadTree<RowIndicatorType>>(bounds);
    this->m_vertex_data.reserve(m_x_coords.size() * 3);// Reserve space for x, y, group_id

    for (size_t i = 0; i < m_x_coords.size(); ++i) {
        CoordType x = m_x_coords[i];
        CoordType y = m_y_coords[i];
        
        // Determine row indicator
        RowIndicatorType row_indicator;
        if (m_use_provided_indicators) {
            row_indicator = m_row_indicators[i];
        } else {
            // Use vector index as row indicator
            row_indicator = static_cast<RowIndicatorType>(i);
        }

        // Store coordinates in QuadTree
        this->m_spatial_index->insert(static_cast<float>(x), static_cast<float>(y), row_indicator);

        // Store coordinates and group_id in vertex data for OpenGL rendering
        this->m_vertex_data.push_back(static_cast<float>(x));
        this->m_vertex_data.push_back(static_cast<float>(y));
        this->m_vertex_data.push_back(0.0f);// group_id = 0 (ungrouped) initially
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

    qDebug() << "VectorPointVisualization: Populated data with" << this->m_total_point_count << "points";
}

template<typename CoordType, typename RowIndicatorType>
BoundingBox VectorPointVisualization<CoordType, RowIndicatorType>::getDataBounds() const {
    if (m_x_coords.empty()) {
        return BoundingBox(0.0f, 0.0f, 1.0f, 1.0f);
    }

    // Find min/max coordinates
    CoordType min_x = m_x_coords[0];
    CoordType max_x = m_x_coords[0];
    CoordType min_y = m_y_coords[0];
    CoordType max_y = m_y_coords[0];

    for (size_t i = 1; i < m_x_coords.size(); ++i) {
        min_x = std::min(min_x, m_x_coords[i]);
        max_x = std::max(max_x, m_x_coords[i]);
        min_y = std::min(min_y, m_y_coords[i]);
        max_y = std::max(max_y, m_y_coords[i]);
    }

    return BoundingBox(static_cast<float>(min_x), static_cast<float>(min_y),
                       static_cast<float>(max_x), static_cast<float>(max_y));
}

#endif// VECTORPOINTVISUALIZATION_HPP 