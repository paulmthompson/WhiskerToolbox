#ifndef PLOT_TOOLTIP_MANAGER_HPP
#define PLOT_TOOLTIP_MANAGER_HPP

/**
 * @file PlotTooltipManager.hpp
 * @brief Shared tooltip manager for plot widgets with dwell-timer hover detection
 *
 * PlotTooltipManager encapsulates the tooltip lifecycle common to all plot
 * widgets: dwell-timer management, stationarity detection, hit testing
 * delegation, and content display. Widgets provide two callbacks:
 *
 *  1. A **hit test** function that converts a screen position to a hit result
 *     (or signals a miss).
 *  2. A **content provider** that turns a hit result into displayable content
 *     (plain text or a rendered QPixmap for glyph-style tooltips).
 *
 * The manager owns the QTimer, handles mouse-move restarts, suppresses
 * tooltips during panning, and hides the tooltip on widget leave. Costly
 * hit tests and content generation only run after the dwell interval elapses,
 * so rapid mouse movement never triggers expensive computation.
 *
 * ## Quick-start (text tooltips)
 *
 * @code
 * // In your OpenGLWidget constructor:
 * _tooltip_mgr = std::make_unique<PlotTooltipManager>(this);
 * _tooltip_mgr->setHitTestProvider([this](QPoint pos) -> std::optional<PlotTooltipHit> {
 *     auto hit = findEventNear(pos);
 *     if (!hit) return std::nullopt;
 *     QPointF world = screenToWorld(pos);
 *     return PlotTooltipHit{
 *         static_cast<float>(world.x()),
 *         static_cast<float>(world.y()),
 *         hit->second};
 * });
 * _tooltip_mgr->setTextProvider([](PlotTooltipHit const & hit) -> QString {
 *     return QString("Time: %1 ms").arg(hit.world_x, 0, 'f', 1);
 * });
 *
 * // In mouseMoveEvent:
 * _tooltip_mgr->onMouseMove(event->pos(), is_panning);
 *
 * // In leaveEvent:
 * _tooltip_mgr->onLeave();
 * @endcode
 *
 * ## Rich tooltips (QPixmap)
 *
 * For widgets that need to render glyphs or mini-plots in their tooltips,
 * set a pixmap provider instead of (or in addition to) the text provider.
 * The pixmap provider receives the hit result and returns a QPixmap that is
 * displayed in a lightweight popup widget near the cursor.
 *
 * @see PlotInteractionHelpers.hpp for screenToWorld / coordinate conversion
 */

#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QString>

#include <any>
#include <functional>
#include <memory>
#include <optional>

class QTimer;
class QWidget;

namespace WhiskerToolbox::Plots {

// =============================================================================
// Hit result
// =============================================================================

/**
 * @brief Result of a tooltip hit test
 *
 * Carries the world coordinates of the hit and an optional user-defined
 * payload for widget-specific data (trial index, series key, etc.).
 */
struct PlotTooltipHit {
    float world_x{0.0f};
    float world_y{0.0f};

    /**
     * @brief Arbitrary payload for widget-specific hit data
     *
     * Use std::any so widgets can attach whatever metadata they need
     * (e.g. trial index, series key, unit name) without the tooltip
     * manager knowing about it.
     */
    std::any user_data;
};

// =============================================================================
// Tooltip content
// =============================================================================

/**
 * @brief Content to display in the tooltip
 *
 * Either plain text (shown via QToolTip::showText) or a pixmap
 * (shown in a lightweight popup widget). At least one must be set.
 */
struct PlotTooltipContent {
    /// Plain text tooltip (used if pixmap is null)
    QString text;

    /// Rich tooltip rendered as an image (takes precedence over text)
    QPixmap pixmap;

    [[nodiscard]] bool hasPixmap() const { return !pixmap.isNull(); }
    [[nodiscard]] bool hasText() const { return !text.isEmpty(); }
    [[nodiscard]] bool isEmpty() const { return !hasPixmap() && !hasText(); }
};

// =============================================================================
// Callback types
// =============================================================================

/**
 * @brief Callback that performs a hit test at a screen position
 *
 * Returns a PlotTooltipHit on success, or std::nullopt if nothing is under
 * the cursor. This callback may perform spatial queries (QuadTree, etc.)
 * and is only called after the dwell timer fires.
 */
using TooltipHitTestFn =
        std::function<std::optional<PlotTooltipHit>(QPoint screen_pos)>;

/**
 * @brief Callback that generates tooltip text from a hit result
 */
using TooltipTextFn =
        std::function<QString(PlotTooltipHit const & hit)>;

/**
 * @brief Callback that generates rich tooltip content from a hit result
 *
 * Use this for expensive content generation (rendering glyphs, mini-plots).
 * Returns a full PlotTooltipContent, allowing the provider to choose between
 * text and pixmap on a per-hit basis.
 */
using TooltipContentFn =
        std::function<PlotTooltipContent(PlotTooltipHit const & hit)>;

// =============================================================================
// PlotTooltipManager
// =============================================================================

/**
 * @brief Manages tooltip lifecycle for plot widgets
 *
 * Owns a single-shot QTimer. On each mouse move the timer is restarted.
 * When the timer fires (mouse is "stationary"), the hit-test provider is
 * called. If a hit is found, the content provider generates the tooltip
 * content, which is then displayed.
 *
 * The manager must be told about panning state so that tooltips are
 * suppressed during drag operations.
 */
class PlotTooltipManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a tooltip manager
     * @param parent_widget The QWidget that owns this manager and anchors the tooltip position
     * @param dwell_ms Dwell time in milliseconds before showing the tooltip (default 500)
     */
    explicit PlotTooltipManager(QWidget * parent_widget, int dwell_ms = 500);
    ~PlotTooltipManager() override;

    // Non-copyable, non-movable
    PlotTooltipManager(PlotTooltipManager const &) = delete;
    PlotTooltipManager & operator=(PlotTooltipManager const &) = delete;
    PlotTooltipManager(PlotTooltipManager &&) = delete;
    PlotTooltipManager & operator=(PlotTooltipManager &&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the hit-test callback
     *
     * This function is called when the dwell timer fires to determine
     * whether anything is under the cursor. It should perform spatial
     * queries (QuadTree, kd-tree, etc.) and return a hit result.
     */
    void setHitTestProvider(TooltipHitTestFn provider);

    /**
     * @brief Set a simple text content provider
     *
     * Convenience overload for widgets that only need plain-text tooltips.
     * Internally wraps the text in a PlotTooltipContent.
     */
    void setTextProvider(TooltipTextFn provider);

    /**
     * @brief Set the full content provider (supports text and pixmap)
     *
     * Use this when the tooltip may contain rendered images (glyphs,
     * mini-plots, etc.).
     */
    void setContentProvider(TooltipContentFn provider);

    /**
     * @brief Set the dwell time before showing the tooltip
     * @param ms Dwell time in milliseconds
     */
    void setDwellTime(int ms);

    /**
     * @brief Enable or disable tooltips entirely
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check whether tooltips are enabled
     */
    [[nodiscard]] bool isEnabled() const;

    // =========================================================================
    // Event forwarding (call from widget event handlers)
    // =========================================================================

    /**
     * @brief Notify the manager of a mouse move
     *
     * Call this from the widget's mouseMoveEvent(). The manager restarts
     * the dwell timer at the new position. If is_interacting is true
     * (e.g. panning or drawing), the timer is suppressed.
     *
     * @param screen_pos Current screen-space cursor position
     * @param is_interacting True if the widget is currently panning/dragging
     */
    void onMouseMove(QPoint screen_pos, bool is_interacting);

    /**
     * @brief Notify the manager that the mouse left the widget
     *
     * Stops the timer and hides any visible tooltip.
     * Call this from leaveEvent().
     */
    void onLeave();

    /**
     * @brief Hide the tooltip immediately
     *
     * Useful when the scene changes (rebuild) or the widget starts
     * an interaction that should dismiss the tooltip.
     */
    void hide();

private slots:
    void onDwellTimerFired();

private:
    void showTextTooltip(QString const & text, QPoint screen_pos);
    void showPixmapTooltip(QPixmap const & pixmap, QPoint screen_pos);
    void hidePixmapPopup();

    QWidget * _parent_widget{nullptr};
    QTimer * _dwell_timer{nullptr};
    bool _enabled{true};
    std::optional<QPoint> _pending_pos;

    // Providers
    TooltipHitTestFn _hit_test_fn;
    TooltipContentFn _content_fn;

    // Pixmap popup (lazily created)
    class PixmapPopup;
    std::unique_ptr<PixmapPopup> _pixmap_popup;
};

}// namespace WhiskerToolbox::Plots

#endif// PLOT_TOOLTIP_MANAGER_HPP
