#include "DigitalTimeSeriesGraph.hpp"

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtprange.h"

DigitalTimeSeriesGraph::DigitalTimeSeriesGraph(JKQTBasePlotter *parent):
    JKQTPPlotElement(parent)
{
    _parent = parent;
}

void DigitalTimeSeriesGraph::load_digital_vector(std::vector<std::pair<float, float>> digital_vector){
    bool first = false;
    QColor color;
    for (auto [start, end] : digital_vector){
        JKQTPVerticalRange* graph = new JKQTPVerticalRange(_parent);
        graph->setPlotCenterLine(false);
        if (!first){
            color = graph->getFillColor();
            color.setAlpha(100);
            first = true;
        }
        graph->setColor(color);
        graph->setRangeMax(end);
        graph->setRangeMin(start);
        _graphs.push_back(graph);
    }
}

void DigitalTimeSeriesGraph::draw(JKQTPEnhancedPainter &painter)
{   
    for (auto graph : _graphs){
        graph->draw(painter);
    }
}

void DigitalTimeSeriesGraph::drawKeyMarker(JKQTPEnhancedPainter& painter, const QRectF& rect){
    for (auto graph : _graphs){
        graph->drawKeyMarker(painter, rect);
    }
}

bool DigitalTimeSeriesGraph::getXMinMax(double& minx, double& maxx, double& smallestGreaterZero){
    if (_graphs.empty()){
        return false;
    }

    double mn = 1e9;
    double mx = -1e9;
    double mn_gz = 1e9;
    for (auto graph : _graphs){
        double cur_mn, cur_mx, cur_mn_gz;
        graph->getXMinMax(cur_mn, cur_mx, cur_mn_gz);

        mn = std::min(mn, cur_mn);
        mx = std::max(mx, cur_mx);
        mn_gz = std::min(mn_gz, cur_mn_gz);
    }
    minx = mn;
    maxx = mx;
    smallestGreaterZero = mn_gz;
    return true;
}

bool DigitalTimeSeriesGraph::getYMinMax(double& miny, double& maxy, double& smallestGreaterZero){
    if (_graphs.empty()){
        return false;
    }

    double mn = 1e9;
    double mx = -1e9;
    double mn_gz = 1e9;
    for (auto graph : _graphs){
        double cur_mn, cur_mx, cur_mn_gz;
        graph->getYMinMax(cur_mn, cur_mx, cur_mn_gz);

        mn = std::min(mn, cur_mn);
        mx = std::max(mx, cur_mx);
        mn_gz = std::min(mn_gz, cur_mn_gz);
    }
    miny = mn;
    maxy = mx;
    smallestGreaterZero = mn_gz;
    return true;
}

QColor DigitalTimeSeriesGraph::getKeyLabelColor() const{
    // i dont know what this is for
    return QColor(0, 255, 0);
}
