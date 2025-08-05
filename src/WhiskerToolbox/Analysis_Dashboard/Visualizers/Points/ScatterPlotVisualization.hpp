#ifndef SCATTERPLOTVISUALIZATION_HPP
#define SCATTERPLOTVISUALIZATION_HPP

#include "GenericPointVisualization.hpp"

#include <QString>
#include <vector>

class GroupManager;

/**
 * @brief Specialized point visualization for scatter plot data
 * 
 * This class extends VectorPointVisualization to provide specific
 * functionality for scatter plots, including tooltip generation
 * and data management for X vs Y plotting.
 */
class ScatterPlotVisualization : public GenericPointVisualization<float, size_t> {
public:
    /**
     * @brief Constructor for scatter plot visualization
     * @param data_key The key identifier for this visualization
     * @param x_coords Vector of X coordinates
     * @param y_coords Vector of Y coordinates
     * @param group_manager Optional group manager for color coding
     * @param defer_opengl_init Whether to defer OpenGL initialization
     */
    ScatterPlotVisualization(QString const & data_key,
                            std::vector<float> const & x_coords,
                            std::vector<float> const & y_coords,
                            GroupManager * group_manager = nullptr,
                            bool defer_opengl_init = false);

    /**
     * @brief Destructor
     */
    ~ScatterPlotVisualization() override = default;

    /**
     * @brief Update the scatter plot data
     * @param x_coords New X coordinates
     * @param y_coords New Y coordinates
     */
    void updateData(std::vector<float> const & x_coords, 
                   std::vector<float> const & y_coords);

    /**
     * @brief Set labels for X and Y axes (used in tooltips)
     * @param x_label Label for X axis
     * @param y_label Label for Y axis
     */
    void setAxisLabels(QString const & x_label, QString const & y_label);

    /**
     * @brief Get the current X axis label
     */
    QString getXAxisLabel() const { return m_x_label; }

    /**
     * @brief Get the current Y axis label
     */
    QString getYAxisLabel() const { return m_y_label; }

protected:
    /**
     * @brief Generate tooltip text for a scatter plot point
     * @param row_indicator The row indicator for the point
     * @return Formatted tooltip text
     */
    QString getTooltipText(size_t row_indicator);

    /**
     * @brief Populate data from vector inputs
     */
    void populateData() override;

    /**
     * @brief Get the bounding box for the vector data
     * @return BoundingBox containing all data points
     */
    BoundingBox getDataBounds() const override;

private:
    QString m_x_label;      ///< Label for X axis
    QString m_y_label;      ///< Label for Y axis
    std::vector<float> m_x_data;  ///< Copy of X data for tooltip generation
    std::vector<float> m_y_data;  ///< Copy of Y data for tooltip generation
};

#endif // SCATTERPLOTVISUALIZATION_HPP
