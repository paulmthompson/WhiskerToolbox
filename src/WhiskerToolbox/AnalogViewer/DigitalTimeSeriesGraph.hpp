#ifndef DIGITAL_TIME_SERIES_GRAPH_HPP
#define DIGITAL_TIME_SERIES_GRAPH_HPP

#include "DataManager/DigitalTimeSeries/interval_data.hpp"

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtprange.h"

#include <vector>



class DigitalTimeSeriesGraph : public JKQTPPlotElement {
    
public:
    DigitalTimeSeriesGraph(JKQTBasePlotter *parent=nullptr);
    ~DigitalTimeSeriesGraph();


    void load_digital_vector(std::vector<Interval> digital_vector);


    void draw(JKQTPEnhancedPainter& painter) override;
    void drawKeyMarker(JKQTPEnhancedPainter& painter, const QRectF& rect) override;
    bool getXMinMax(double& minx, double& maxx, double& smallestGreaterZero) override;
    bool getYMinMax(double& miny, double& maxy, double& smallestGreaterZero) override;
    QColor getKeyLabelColor() const override;

    void setColor(QColor color);
    void setLineStyle(Qt:: PenStyle style);

private:
    std::vector<JKQTPVerticalRange*> _graphs;

    JKQTBasePlotter* _parent;
};

#endif
