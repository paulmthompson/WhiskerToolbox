#pragma once

#include <QObject>
#include <QTimer>
#include <QPoint>
#include <QString>
#include <functional>
#include <optional>

/**
 * @brief Manages tooltip functionality for plot widgets
 * 
 * This class handles the timing, positioning, and display of tooltips
 * for plot widgets. It provides debounced tooltip display with refresh
 * functionality to keep tooltips visible during mouse movement.
 */
class TooltipManager : public QObject {
    Q_OBJECT

public:
    using TooltipContentProvider = std::function<std::optional<QString>(QPoint const& screen_pos)>;

    explicit TooltipManager(QObject* parent = nullptr);
    ~TooltipManager() = default;

    /**
     * @brief Enable or disable tooltips
     * @param enabled Whether tooltips should be shown
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if tooltips are enabled
     * @return True if tooltips are enabled
     */
    bool isEnabled() const { return _enabled; }

    /**
     * @brief Set the function that provides tooltip content
     * @param provider Function that takes screen position and returns tooltip text
     */
    void setContentProvider(TooltipContentProvider provider);

    /**
     * @brief Set tooltip timing parameters
     * @param show_delay_ms Delay before showing tooltip (default: 500ms)
     * @param refresh_interval_ms Interval for refreshing tooltip (default: 100ms)
     */
    void setTiming(int show_delay_ms, int refresh_interval_ms);

    /**
     * @brief Handle mouse movement - start or restart tooltip timer
     * @param screen_pos Mouse position in screen coordinates
     */
    void handleMouseMove(QPoint const& screen_pos);

    /**
     * @brief Handle mouse leave - hide tooltip
     */
    void handleMouseLeave();

    /**
     * @brief Force hide tooltip immediately
     */
    void hideTooltip();

    /**
     * @brief Temporarily disable tooltips (e.g., during interaction)
     */
    void setSuppressed(bool suppressed);

private slots:
    void handleShowTooltip();
    void handleRefreshTooltip();

private:
    bool _enabled = true;
    bool _suppressed = false;
    
    QTimer* _show_timer;
    QTimer* _refresh_timer;
    
    QPoint _current_mouse_pos;
    TooltipContentProvider _content_provider;
    
    int _show_delay_ms = 500;
    int _refresh_interval_ms = 100;
    
    void stopAllTimers();
};
