#include "DigitalTimeSeriesGraph.hpp"

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtprange.h"

/**
 * @brief Constructor for DigitalTimeSeriesGraph
 * @param parent Parent plotter
 */
DigitalTimeSeriesGraph::DigitalTimeSeriesGraph(JKQTBasePlotter *parent):
    JKQTPPlotElement(parent)
{
    _parent = parent;
}

/**
 * @brief Load a digital time series into the graph and create a vertical range for each ON range
 * @param digital_vector Vector of pairs of floats representing the start and end of the ON range
 */
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

/**
 * @brief Draw the digital time series graph by calling draw on each vertical range
 * @param painter Painter object
 */
void DigitalTimeSeriesGraph::draw(JKQTPEnhancedPainter &painter)
{   
    for (auto graph : _graphs){
        graph->draw(painter);
    }
}

/**
 * @brief Draw the key marker for the digital time series graph by calling drawKeyMarker on each vertical range
 * @param painter Painter object
 */
void DigitalTimeSeriesGraph::drawKeyMarker(JKQTPEnhancedPainter& painter, const QRectF& rect){
    for (auto graph : _graphs){
        graph->drawKeyMarker(painter, rect);
    }
}

/**
 * @brief Get the minimum, maximum, and minimum greater than zero x values of the digital time series graph
 * @param minx Minimum x value as reference
 * @param maxx Maximum x value as reference
 * @param smallestGreaterZero Minimum x value greater than zero as reference
 */
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

/**
 * @brief Get the minimum, maximum, and minimum greater than zero y values of the digital time series graph
 * @param miny Minimum y value as reference
 * @param maxy Maximum y value as reference
 * @param smallestGreaterZero Minimum y value greater than zero as reference
 */
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

/**
 * @brief Get the color of the key label for the digital time series graph
 * @return Color of the key label
 */
QColor DigitalTimeSeriesGraph::getKeyLabelColor() const{
    // i dont know what this is for
    return QColor(0, 255, 0);
}

void DigitalTimeSeriesGraph::setColor(const QColor& color){
    for (auto graph : _graphs){
        graph->setColor(color);
    }
}

void DigitalTimeSeriesGraph::setLineStyle(Qt::PenStyle style){
    for (auto graph : _graphs){
        graph->setLineStyle(style);
    }
}