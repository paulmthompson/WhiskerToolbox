#ifndef DOCKINGPLOTORGANIZER_HPP
#define DOCKINGPLOTORGANIZER_HPP

// This file defines a plot organizer that hosts each plot inside its own
// Advanced Docking System dock widget, integrating with the application's
// global dock manager.

// The organizer wraps existing QGraphicsItem-based plots per dock using a
// per-plot QGraphicsView with a QOpenGLWidget viewport, so existing plot
// rendering code remains unchanged.

// Lakos include order
// 1) Prototype header for this implementation: none
// 2) Other project headers
#include "Analysis_Dashboard/PlotOrganizers/AbstractPlotOrganizer.hpp"
// 3) Third-party/non-standard libraries
// Forward declarations for ADS to avoid heavy includes in the header
namespace ads {
class CDockManager;
class CDockWidget;
}
// 4) Almost-standard libraries: none
// 5) C++ standard library
#include <map>
#include <memory>

class PlotContainer;
class PlotDockWidgetContent;

/**
 * @brief Organizer that docks each plot as a separate dock widget
 *
 * This organizer integrates with the application's global ads::CDockManager
 * so plots can be docked/floated freely. Each plot is rendered inside its own
 * QGraphicsView with a QOpenGLWidget viewport.
 */
class DockingPlotOrganizer : public AbstractPlotOrganizer {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DockingPlotOrganizer
     * @param dock_manager Pointer to the global dock manager
     * @param parent QObject parent
     * @pre dock_manager may be nullptr; in that case, addPlot will be a no-op
     * @post Organizer is ready to accept plots
     */
    explicit DockingPlotOrganizer(ads::CDockManager* dock_manager, QObject* parent = nullptr);
    ~DockingPlotOrganizer() override;

    // AbstractPlotOrganizer interface
    void addPlot(std::unique_ptr<PlotContainer> plot_container) override;
    bool removePlot(QString const& plot_id) override;
    PlotContainer* getPlot(QString const& plot_id) const override;
    QStringList getAllPlotIds() const override;
    int getPlotCount() const override;
    void selectPlot(QString const& plot_id) override;
    void clearAllPlots() override;
    QWidget* getDisplayWidget() override;

private slots:
    /**
     * @brief Handle activation (focus/click) within a plot dock content
     * @param plot_id The plot identifier
     * @pre plot_id must refer to a known plot
     * @post The plot is considered selected and plotSelected emitted
     */
    void _onContentActivated(QString const& plot_id);

    /**
     * @brief Handle dock close requests to remove plots
     * @param plot_id The plot identifier
     * @pre plot_id must refer to a known plot
     * @post Plot is removed and plotRemoved emitted
     */
    void _onDockCloseRequested(QString const& plot_id);

    /**
     * @brief Forward container plot selection to organizer signal
     */
    void _onPlotSelected(QString const& plot_id);

    /**
     * @brief Forward container frame jump to organizer signal
     */
    void _onFrameJumpRequested(int64_t time_frame_index, const std::string& data_key);

private:
    struct Entry {
        std::unique_ptr<PlotContainer> _container;
        PlotDockWidgetContent* _content; // not owning
        ads::CDockWidget* _dock;         // not owning
    };

    ads::CDockManager* _dock_manager;
    QWidget* _placeholder_widget;
    std::map<QString, Entry> _entries;

    /**
     * @brief Connect signals from a plot container
     * @param container Container to connect
     * @pre container != nullptr
     * @post Signals connected
     */
    void _connectContainerSignals(PlotContainer* container);

    /**
     * @brief Disconnect signals from a plot container
     * @param container Container to disconnect
     * @pre container != nullptr
     * @post Signals disconnected
     */
    void _disconnectContainerSignals(PlotContainer* container);
};

#endif // DOCKINGPLOTORGANIZER_HPP

