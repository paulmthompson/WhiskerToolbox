#ifndef DIGITAL_TIME_SERIES_GRAPH_HPP
#define DIGITAL_TIME_SERIES_GRAPH_HPP

#include <vector>

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtprange.h"

class DigitalTimeSeriesGraph : public JKQTPPlotElement {
    
public:
    DigitalTimeSeriesGraph(JKQTBasePlotter *parent=nullptr);


    void load_digital_vector(std::vector<std::pair<float, float>> digital_vector);


    void draw(JKQTPEnhancedPainter& painter) override;
    void drawKeyMarker(JKQTPEnhancedPainter& painter, const QRectF& rect) override;
    bool getXMinMax(double& minx, double& maxx, double& smallestGreaterZero) override;
    bool getYMinMax(double& miny, double& maxy, double& smallestGreaterZero) override;
    QColor getKeyLabelColor() const override;

private:
    std::vector<JKQTPVerticalRange*> _graphs;

    JKQTBasePlotter* _parent;
};

#endif