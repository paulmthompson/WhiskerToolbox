#ifndef ABSTRACTPLOTPARAMETERS_HPP
#define ABSTRACTPLOTPARAMETERS_HPP

#include <memory>
#include <string>

class DataSourceRegistry;
class GroupManager;

/**
 * @brief Parameters struct for AbstractPlotWidget that has no Qt dependencies
 * 
 * This struct encapsulates the state of a plot widget without any Qt dependencies,
 * making it easier to separate concerns and potentially serialize/deserialize
 * plot configurations.
 */
struct AbstractPlotParameters {
    /**
     * @brief Default constructor
     */
    AbstractPlotParameters();

    /**
     * @brief Constructor with initial values
     * @param plot_title The title of the plot
     */
    explicit AbstractPlotParameters(std::string const & plot_title);

    /**
     * @brief Generate a unique ID for this plot instance
     * @return The generated unique ID
     */
    std::string generateUniqueId();

    /**
     * @brief Get the unique identifier for this plot instance
     * @return Unique ID string
     */
    std::string getPlotId() const;

    /**
     * @brief Set the plot instance name/title
     * @param title The new title for this plot
     */
    void setPlotTitle(std::string const & title);

    /**
     * @brief Get the plot instance name/title
     * @return The specific name/title for this plot instance
     */
    std::string getPlotTitle() const;

    DataSourceRegistry * data_source_registry = nullptr;
    GroupManager * group_manager = nullptr;
    std::string plot_title;
    std::string plot_id;

private:
    static int next_plot_id;
};

#endif// ABSTRACTPLOTPARAMETERS_HPP