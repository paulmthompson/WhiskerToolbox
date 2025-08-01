#ifndef PLOTFACTORY_HPP
#define PLOTFACTORY_HPP

#include <QString>
#include <memory>

class AbstractPlotWidget;
class AbstractPlotPropertiesWidget;
class PlotContainer;
class DataManager;
class GroupManager;
class TableManager;

/**
 * @brief Factory for creating plot widgets and their associated properties widgets
 * 
 * This factory encapsulates the creation logic for different plot types,
 * ensuring that each plot is created with its appropriate properties widget.
 */
class PlotFactory {
public:
    /**
     * @brief Create a complete plot container with both plot and properties widgets
     * @param plot_type The type of plot to create (e.g., "scatter_plot", "spatial_overlay_plot")
     * @return Unique pointer to the created plot container, or nullptr if type is unknown
     */
    static std::unique_ptr<PlotContainer> createPlotContainer(const QString& plot_type);

private:
    /**
     * @brief Create a plot widget of the specified type
     * @param plot_type The type of plot to create
     * @return Unique pointer to the created plot widget, or nullptr if type is unknown
     */
    static std::unique_ptr<AbstractPlotWidget> createPlotWidget(const QString& plot_type);

    /**
     * @brief Create a properties widget for the specified plot type
     * @param plot_type The type of plot properties widget to create
     * @return Unique pointer to the created properties widget, or nullptr if type is unknown
     */
    static std::unique_ptr<AbstractPlotPropertiesWidget> createPropertiesWidget(const QString& plot_type);
};

#endif // PLOTFACTORY_HPP
