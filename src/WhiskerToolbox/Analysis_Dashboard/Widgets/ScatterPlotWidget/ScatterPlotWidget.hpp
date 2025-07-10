#ifndef SCATTERPLOTWIDGET_HPP
#define SCATTERPLOTWIDGET_HPP

#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"

#include <QPainter>

/**
 * @brief Placeholder implementation of a scatter plot widget
 * 
 * This serves as an example implementation of AbstractPlotWidget
 * and provides a basic scatter plot visualization.
 */
class ScatterPlotWidget : public AbstractPlotWidget {
    Q_OBJECT

public:
    explicit ScatterPlotWidget(QGraphicsItem* parent = nullptr);
    ~ScatterPlotWidget() override = default;

    QString getPlotType() const override;

protected:
    /**
     * @brief Paint the scatter plot
     */
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

private:
    // Placeholder data for demonstration
    QList<QPointF> _sample_points;
    
    void generateSampleData();
};

#endif // SCATTERPLOTWIDGET_HPP 