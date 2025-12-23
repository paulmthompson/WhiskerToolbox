#ifndef DATAVIEWER_TOOLTIP_CONTROLLER_HPP
#define DATAVIEWER_TOOLTIP_CONTROLLER_HPP

/**
 * @file DataViewerTooltipController.hpp
 * @brief Manages tooltip display for the DataViewer widget
 * 
 * This class extracts tooltip management from OpenGLWidget to provide
 * a cleaner separation of concerns. It handles:
 * - Hover delay timing
 * - Series info lookup via callback
 * - Tooltip text formatting and display
 * - Tooltip cancellation on mouse movement
 * 
 * The controller uses callbacks to look up series information, avoiding
 * tight coupling with the parent widget's data structures.
 */

#include <QObject>
#include <QPoint>
#include <QTimer>

#include <functional>
#include <optional>
#include <string>

class QWidget;

namespace DataViewer {

/**
 * @brief Information about a series at a specific position
 */
struct SeriesInfo {
    std::string type;    ///< Series type ("Analog", "Event", "Interval")
    std::string key;     ///< Series key/identifier
    float value{0.0f};   ///< Optional value (for analog series at hover position)
    bool has_value{false}; ///< Whether value field is meaningful
};

/**
 * @brief Manages tooltip display for the DataViewer widget
 * 
 * Handles the timing and display of tooltips when hovering over series.
 * Uses callbacks to look up series information without tight coupling
 * to the widget's data structures.
 */
class DataViewerTooltipController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a tooltip controller
     * @param parent_widget The widget to display tooltips on (used for coordinate mapping)
     */
    explicit DataViewerTooltipController(QWidget * parent_widget);
    ~DataViewerTooltipController() override = default;

    /**
     * @brief Set the delay before showing tooltips
     * @param delay_ms Delay in milliseconds (default is 1000ms)
     */
    void setDelay(int delay_ms);

    /**
     * @brief Get the current tooltip delay
     * @return Delay in milliseconds
     */
    [[nodiscard]] int delay() const;

    /**
     * @brief Schedule a tooltip to appear at the given position
     * 
     * Starts the hover timer. If the timer completes before cancel() is called,
     * the tooltip will be displayed. If scheduleTooltip is called again before
     * the timer completes, the timer restarts with the new position.
     * 
     * @param canvas_pos Position in canvas coordinates where hover occurred
     */
    void scheduleTooltip(QPoint const & canvas_pos);

    /**
     * @brief Cancel any pending tooltip
     * 
     * Stops the hover timer and hides any visible tooltip.
     */
    void cancel();

    /**
     * @brief Check if a tooltip is currently scheduled (timer running)
     * @return true if waiting to show a tooltip
     */
    [[nodiscard]] bool isScheduled() const;

    /**
     * @brief Callback type for finding series at a position
     * 
     * The callback receives canvas coordinates and returns optional SeriesInfo
     * describing the series (if any) at that position.
     */
    using SeriesInfoProvider = std::function<std::optional<SeriesInfo>(float canvas_x, float canvas_y)>;

    /**
     * @brief Set the callback used to look up series information
     * 
     * This callback is invoked when the tooltip timer fires to determine
     * what information to display.
     * 
     * @param provider Function that returns SeriesInfo for a canvas position
     */
    void setSeriesInfoProvider(SeriesInfoProvider provider);

signals:
    /**
     * @brief Emitted when a tooltip is about to be shown
     * @param pos Canvas position where tooltip will appear
     * @param info The series information that will be displayed
     */
    void tooltipShowing(QPoint const & pos, SeriesInfo const & info);

    /**
     * @brief Emitted when the tooltip is hidden
     */
    void tooltipHidden();

private slots:
    /**
     * @brief Called when the hover timer fires
     * 
     * Looks up series info at the hover position and displays the tooltip.
     */
    void showTooltip();

private:
    /**
     * @brief Format tooltip text from series info
     * @param info Series information to format
     * @return HTML-formatted tooltip text
     */
    [[nodiscard]] static QString formatTooltipText(SeriesInfo const & info);

    QWidget * _parent_widget{nullptr};
    QTimer * _timer{nullptr};
    QPoint _hover_pos;
    SeriesInfoProvider _info_provider;

    static constexpr int DEFAULT_DELAY_MS = 1000;
};

}// namespace DataViewer

#endif// DATAVIEWER_TOOLTIP_CONTROLLER_HPP
