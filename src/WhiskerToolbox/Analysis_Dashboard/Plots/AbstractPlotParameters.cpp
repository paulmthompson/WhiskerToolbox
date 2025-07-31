#include "AbstractPlotParameters.hpp"

#include <sstream>

// Static member initialization
int AbstractPlotParameters::next_plot_id = 1;

AbstractPlotParameters::AbstractPlotParameters()
    : plot_title("Untitled Plot") {
    generateUniqueId();
}

AbstractPlotParameters::AbstractPlotParameters(std::string const & plot_title)
    : plot_title(plot_title) {
    generateUniqueId();
}

std::string AbstractPlotParameters::generateUniqueId() {
    std::ostringstream oss;
    oss << "plot_" << next_plot_id++;
    plot_id = oss.str();
    return plot_id;
}

std::string AbstractPlotParameters::getPlotId() const {
    return plot_id;
}

void AbstractPlotParameters::setPlotTitle(std::string const & title) {
    plot_title = title;
}

std::string AbstractPlotParameters::getPlotTitle() const {
    return plot_title;
}