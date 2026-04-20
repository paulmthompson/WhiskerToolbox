#ifndef TIME_CONTROLLER_HPP
#define TIME_CONTROLLER_HPP

/**
 * @file TimeController.hpp
 * @brief Qt-free holder for the current visualization time and active time key
 *
 * Owns `TimePosition`, optional active `TimeKey`, and a re-entrancy guard matching
 * the former `EditorRegistry` time update behavior. Notifies changes via
 * `std::function` callbacks for bridging into Qt or command execution.
 */

#include "StrongTimeTypes.hpp"
#include "TimeFrame.hpp"

#include <functional>
#include <memory>

/**
 * @brief Owns global visualization time state without Qt dependencies
 */
class TimeController {
public:
    using TimeChangedCallback = std::function<void(TimePosition const &)>;
    using TimeKeyChangedCallback = std::function<void(TimeKey new_key, TimeKey old_key)>;

    /**
     * @brief Set the current visualization time
     * @param position New time position (index and clock identity)
     */
    void setCurrentTime(TimePosition const & position);

    /**
     * @brief Set the current visualization time from index and timeframe
     * @param index Index within the timeframe
     * @param tf Timeframe shared pointer (may be null)
     */
    void setCurrentTime(TimeFrameIndex index, std::shared_ptr<TimeFrame> tf);

    /**
     * @return Current visualization time position
     */
    [[nodiscard]] TimePosition currentPosition() const;

    /**
     * @return Current frame index component
     */
    [[nodiscard]] TimeFrameIndex currentTimeIndex() const;

    /**
     * @return Current timeframe pointer (may be null)
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> currentTimeFrame() const;

    /**
     * @brief Select which logical timeframe key is active for the UI
     * @param key New active key; no-op if equal to the current key
     */
    void setActiveTimeKey(TimeKey key);

    /**
     * @return The active timeframe key (defaults to @c "time")
     */
    [[nodiscard]] TimeKey activeTimeKey() const;

    /**
     * @brief Replace the time-changed notification callback
     * @param cb Callback invoked after a successful time update, or empty to clear
     */
    void setOnTimeChanged(TimeChangedCallback cb);

    /**
     * @brief Replace the active-time-key notification callback
     * @param cb Callback invoked when the active key changes, or empty to clear
     */
    void setOnTimeKeyChanged(TimeKeyChangedCallback cb);

private:
    TimePosition _current_position;
    TimeKey _active_time_key{TimeKey("time")};
    bool _time_update_in_progress{false};
    TimeChangedCallback _on_time_changed;
    TimeKeyChangedCallback _on_time_key_changed;
};

#endif// TIME_CONTROLLER_HPP
