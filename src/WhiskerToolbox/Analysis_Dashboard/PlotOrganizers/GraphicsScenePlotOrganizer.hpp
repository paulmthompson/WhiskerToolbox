#ifndef GRAPHICSSCENEPLOTORGANIZER_HPP
#define GRAPHICSSCENEPLOTORGANIZER_HPP

#include "AbstractPlotOrganizer.hpp"

#include <QPointF>

#include <map>
#include <memory>

class PlotContainer;
class QGraphicsScene;
class QGraphicsView;

/**
 * @brief Plot organizer that uses QGraphicsScene for layout
 * 
 * This is a concrete implementation of AbstractPlotOrganizer that maintains
 * the current QGraphicsScene-based approach. Plots can be dragged, resized,
 * and positioned freely within the scene.
 */
class GraphicsScenePlotOrganizer : public AbstractPlotOrganizer {
    Q_OBJECT

public:
    explicit GraphicsScenePlotOrganizer(QObject * parent = nullptr);
    ~GraphicsScenePlotOrganizer() override = default;

    // AbstractPlotOrganizer interface
    void addPlot(std::unique_ptr<PlotContainer> plot_container) override;
    bool removePlot(QString const & plot_id) override;
    PlotContainer * getPlot(QString const & plot_id) const override;
    QStringList getAllPlotIds() const override;
    int getPlotCount() const override;
    void selectPlot(QString const & plot_id) override;
    void clearAllPlots() override;
    QWidget * getDisplayWidget() override;

    /**
     * @brief Get the graphics scene used by this organizer
     * @return Pointer to the graphics scene
     */
    QGraphicsScene * getScene() const { return scene_; }

    /**
     * @brief Get the graphics view used by this organizer
     * @return Pointer to the graphics view
     */
    QGraphicsView * getView() const { return view_; }

    /**
     * @brief Set the position where new plots should be added
     * @param position The default position for new plots
     */
    void setDefaultPlotPosition(QPointF const & position);

    /**
     * @brief Automatically arrange plots in a grid layout
     */
    void arrangeInGrid();

    /**
     * @brief Ensure all plots are visible within the scene bounds
     */
    void ensurePlotsVisible();

private slots:
    /**
     * @brief Handle plot selection from containers
     * @param plot_id The ID of the selected plot
     */
    void onPlotSelected(QString const & plot_id);

    /**
     * @brief Handle frame jump requests from containers
     * @param time_frame_index The frame index
     * @param data_key The data key
     */
    void onFrameJumpRequested(int64_t time_frame_index, std::string const & data_key);

private:
    QGraphicsScene * scene_;
    QGraphicsView * view_;
    std::map<QString, std::unique_ptr<PlotContainer>> plot_containers_;
    QPointF default_position_;
    QPointF next_position_;

    /**
     * @brief Calculate the next position for a new plot
     * @return The position where the next plot should be placed
     */
    QPointF calculateNextPosition();

    /**
     * @brief Connect signals from a plot container
     * @param container The container to connect
     */
    void connectContainerSignals(PlotContainer * container);

    /**
     * @brief Disconnect signals from a plot container
     * @param container The container to disconnect
     */
    void disconnectContainerSignals(PlotContainer * container);
};

#endif// GRAPHICSSCENEPLOTORGANIZER_HPP
