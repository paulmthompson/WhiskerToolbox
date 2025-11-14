#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "DigitalTimeSeries/EventWithId.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <utility>
#include <vector>

class EntityRegistry;


/**
 * @brief Digital Event Series class
 *
 * A digital event series is a series of events where each event is defined by a time.
 * (Compare to DigitalIntervalSeries which is a series of intervals with start and end times)
 *
 * Use digital events where you wish to specify a time for each event.
 *
 *
 */
class DigitalEventSeries : public ObserverData {
public:
    DigitalEventSeries() = default;
    explicit DigitalEventSeries(std::vector<TimeFrameIndex> event_vector);

    [[nodiscard]] std::vector<TimeFrameIndex> const & getEventSeries() const;

    void addEvent(TimeFrameIndex event_time);

    bool removeEvent(TimeFrameIndex event_time);

    [[nodiscard]] size_t size() const { return _data.size(); }

    void clear() {
        _data.clear();
        notifyObservers();
    }

    [[nodiscard]] auto getEventsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
        return _data | std::views::filter([start_time, stop_time](TimeFrameIndex time) {
                   return time >= start_time && time <= stop_time;
               });
    }

    [[nodiscard]] auto getEventsInRange(TimeFrameIndex start_index,
                                        TimeFrameIndex stop_index,
                                        TimeFrame const & source_time_frame) const {
        if (&source_time_frame == _time_frame.get()) {
            return getEventsInRange(start_index, stop_index);
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame.get()) {
            return getEventsInRange(start_index, stop_index);
        }

        // Use helper function for time frame conversion
        auto [target_start_index, target_stop_index] = convertTimeFrameRange(start_index, stop_index, source_time_frame, *_time_frame);
        return getEventsInRange(target_start_index, target_stop_index);
    };

    std::vector<TimeFrameIndex> getEventsAsVector(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
        std::vector<TimeFrameIndex> result;
        for (TimeFrameIndex time: _data) {
            if (time >= start_time && time <= stop_time) {
                result.push_back(time);
            }
        }
        return result;
    }

    // ========== Events with EntityIDs ==========

    /**
     * @brief Get events in range with their EntityIDs using TimeFrameIndex
     * 
     * @param start_time Start time index for the range
     * @param stop_time Stop time index for the range
     * @return std::vector<EventWithId> Vector of events with their EntityIDs
     */
    [[nodiscard]] std::vector<EventWithId> getEventsWithIdsInRange(TimeFrameIndex start_time,
                                                                   TimeFrameIndex stop_time) const;

    /**
     * @brief Get events in range with their EntityIDs using time frame conversion
     * 
     * @param start_index Start time index in source timeframe
     * @param stop_index Stop time index in source timeframe
     * @param source_time_frame Source timeframe for the indices
     * @return std::vector<EventWithId> Vector of events with their EntityIDs
     */
    [[nodiscard]] std::vector<EventWithId> getEventsWithIdsInRange(TimeFrameIndex start_index,
                                                                   TimeFrameIndex stop_index,
                                                                   TimeFrame const & source_time_frame) const;

    // ========== Time Frame ==========

    /**
     * @brief Set the time frame
     * 
     * @param time_frame The time frame to set
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { _time_frame = time_frame; }

    /**
     * @brief Get the time frame
     * 
     * @return std::shared_ptr<TimeFrame> The current time frame
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const { return _time_frame; }

    // ===== Identity =====
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
        _identity_data_key = data_key;
        _identity_registry = registry;
    }
    void rebuildAllEntityIds();
    [[nodiscard]] std::vector<EntityId> const & getEntityIds() const { return _entity_ids; }

private:
    std::vector<TimeFrameIndex> _data{};
    std::shared_ptr<TimeFrame> _time_frame{nullptr};

    // Sort the events in ascending order
    void _sortEvents();

    // Identity
    std::string _identity_data_key;
    EntityRegistry * _identity_registry{nullptr};
    std::vector<EntityId> _entity_ids;

};

#endif//BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
