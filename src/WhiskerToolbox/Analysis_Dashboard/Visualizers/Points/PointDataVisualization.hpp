#ifndef POINTDATAVISUALIZATION_HPP
#define POINTDATAVISUALIZATION_HPP

#include "GenericPointVisualization.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Points/utils/Point_Data_utils.hpp"

#include <memory>

class PointData;

/**
 * @brief Specific implementation of point visualization for PointData
 * 
 * This class inherits from GenericPointVisualization and provides
 * the specific implementation for working with PointData objects.
 */
struct PointDataVisualization : public GenericPointVisualization<float, int64_t> {
public:
    /**
     * @brief Constructor for PointData visualization
     * @param data_key The key identifier for this visualization
     * @param point_data The PointData object to visualize
     * @param group_manager Optional group manager for color coding
     */
    PointDataVisualization(QString const & data_key,
                           std::shared_ptr<PointData> const & point_data,
                           GroupManager * group_manager = nullptr);

    /**
     * @brief Destructor
     */
    ~PointDataVisualization() override = default;

protected:
    /**
     * @brief Populate data from PointData object
     * @pre The PointData object must be valid
     * @post The spatial index and vertex data will be populated
     */
    void populateData() override;

    /**
     * @brief Get the bounding box for the PointData
     * @return BoundingBox containing all data points
     */
    BoundingBox getDataBounds() const override;

private:
    std::shared_ptr<PointData> m_point_data;
};

#endif// POINTDATAVISUALIZATION_HPP