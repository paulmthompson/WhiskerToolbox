#ifndef ABSTRACTPLOTORGANIZER_HPP
#define ABSTRACTPLOTORGANIZER_HPP

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class PlotContainer;

/**
 * @brief Abstract interface for different plot organization strategies
 * 
 * This interface allows the dashboard to support different ways of organizing
 * plots (QGraphicsScene, dock widgets, tabs, etc.) without changing the core
 * plot logic. Implementations handle the specific UI layout and interaction
 * patterns for their organization method.
 */
class AbstractPlotOrganizer : public QObject {
    Q_OBJECT

public:
    explicit AbstractPlotOrganizer(QObject * parent = nullptr)
        : QObject(parent) {}
    virtual ~AbstractPlotOrganizer() = default;

    /**
     * @brief Add a plot container to the organizer
     * @param plot_container The plot container to add (ownership transferred)
     */
    virtual void addPlot(std::unique_ptr<PlotContainer> plot_container) = 0;

    /**
     * @brief Remove a plot by its ID
     * @param plot_id The unique ID of the plot to remove
     * @return True if plot was found and removed, false otherwise
     */
    virtual bool removePlot(QString const & plot_id) = 0;

    /**
     * @brief Get a plot container by its ID
     * @param plot_id The unique ID of the plot
     * @return Pointer to the plot container, or nullptr if not found
     */
    virtual PlotContainer * getPlot(QString const & plot_id) const = 0;

    /**
     * @brief Get all plot IDs managed by this organizer
     * @return List of all plot IDs
     */
    virtual QStringList getAllPlotIds() const = 0;

    /**
     * @brief Get the number of plots
     * @return Number of plots currently managed
     */
    virtual int getPlotCount() const = 0;

    /**
     * @brief Select a specific plot (if supported by the organization method)
     * @param plot_id The ID of the plot to select
     */
    virtual void selectPlot(QString const & plot_id) = 0;

    /**
     * @brief Clear all plots from the organizer
     */
    virtual void clearAllPlots() = 0;

    /**
     * @brief Get the widget that should be added to the dashboard's layout
     * @return The main widget for this organizer (e.g., QGraphicsView, QWidget, etc.)
     */
    virtual QWidget * getDisplayWidget() = 0;

signals:
    /**
     * @brief Emitted when a plot is selected
     * @param plot_id The unique ID of the selected plot
     */
    void plotSelected(QString const & plot_id);

    /**
     * @brief Emitted when a plot is added
     * @param plot_id The unique ID of the added plot
     */
    void plotAdded(QString const & plot_id);

    /**
     * @brief Emitted when a plot is removed
     * @param plot_id The unique ID of the removed plot
     */
    void plotRemoved(QString const & plot_id);

    /**
     * @brief Emitted when user requests to jump to a specific frame
     * @param time_frame_index The frame index to jump to
     * @param data_key The data key associated with the jump
     */
    void frameJumpRequested(int64_t time_frame_index, std::string const & data_key);

protected:
    /**
     * @brief Helper method to emit plotAdded signal
     * @param plot_id The ID of the added plot
     */
    void emitPlotAdded(QString const & plot_id) { emit plotAdded(plot_id); }

    /**
     * @brief Helper method to emit plotRemoved signal
     * @param plot_id The ID of the removed plot
     */
    void emitPlotRemoved(QString const & plot_id) { emit plotRemoved(plot_id); }

    /**
     * @brief Helper method to emit plotSelected signal
     * @param plot_id The ID of the selected plot
     */
    void emitPlotSelected(QString const & plot_id) { emit plotSelected(plot_id); }

    /**
     * @brief Helper method to emit frameJumpRequested signal
     * @param time_frame_index The frame index
     * @param data_key The data key
     */
    void emitFrameJumpRequested(int64_t time_frame_index, std::string const & data_key) {
        emit frameJumpRequested(time_frame_index, data_key);
    }
};

#endif// ABSTRACTPLOTORGANIZER_HPP
