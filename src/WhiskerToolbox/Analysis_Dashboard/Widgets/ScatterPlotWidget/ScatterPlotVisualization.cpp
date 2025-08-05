#include "ScatterPlotVisualization.hpp"

#include "../../Groups/GroupManager.hpp"

#include <QDebug>

ScatterPlotVisualization::ScatterPlotVisualization(
    QString const & data_key,
    std::vector<float> const & x_coords,
    std::vector<float> const & y_coords,
    GroupManager * group_manager)
    : VectorPointVisualization<float, size_t>(
        data_key, 
        x_coords, 
        y_coords, 
        {}, // Use default row indicators (indices)
        group_manager),
      m_x_label("X"),
      m_y_label("Y"),
      m_x_data(x_coords),
      m_y_data(y_coords) {
    
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

    // Since VectorPointVisualization doesn't have an updateData method,
    // we need to recreate the object or repopulate manually
    // For now, we'll update our internal data and call populateData from the base class
    
    // The issue is that VectorPointVisualization has private members
    // So we need to work around this limitation
    // We'll store our own copy and override the getTooltipText method
    
    qDebug() << "ScatterPlotVisualization: Updated data with" 
             << x_coords.size() << "points";
    
    // TODO: This is a limitation - VectorPointVisualization doesn't support
    // updating data after construction. We'll need to address this or
    // recreate the visualization object when data changes.
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
