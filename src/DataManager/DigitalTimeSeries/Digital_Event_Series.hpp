#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"

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
    explicit DigitalEventSeries(std::vector<float> event_vector);

    void setData(std::vector<float> event_vector);
    [[nodiscard]] std::vector<float> const & getEventSeries() const;

    void addEvent(float event_time);

    bool removeEvent(float event_time);

    [[nodiscard]] size_t size() const { return _data.size(); }

    void clear() {
        _data.clear();
        notifyObservers();
    }

    [[nodiscard]] auto getEventsInRange(float start_time, float stop_time) const {
        return _data | std::views::filter([start_time, stop_time](float time) {
                   return time >= start_time && time <= stop_time;
               });
    }

    [[nodiscard]] auto getEventsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
        return _data | std::views::filter([start_time, stop_time](float time) {
                   return time >= static_cast<float>(start_time.getValue()) && time <= static_cast<float>(stop_time.getValue());
               });
    }

    [[nodiscard]] auto getEventsInRange(TimeFrameIndex start_index,
                                        TimeFrameIndex stop_index,
                                        TimeFrame const * source_time_frame,
                                        TimeFrame const * event_time_frame) const {
        if (source_time_frame == event_time_frame) {
            return getEventsInRange(start_index, stop_index);
        }

        // If either timeframe is null, fall back to original behavior
        if (!source_time_frame || !event_time_frame) {
            return getEventsInRange(start_index, stop_index);
        }

        // Use helper function for time frame conversion
        auto [target_start_index, target_stop_index] = _convertTimeFrameRange(start_index, stop_index, source_time_frame, event_time_frame);
        return getEventsInRange(target_start_index, target_stop_index);
    };

    template<typename TransformFunc>
    auto getEventsInRange(float start_time, float stop_time, TransformFunc time_transform) const {
        return _data | std::views::filter([start_time, stop_time, time_transform](float time) {
                   auto transformed_time = time_transform(time);
                   return transformed_time >= start_time && transformed_time <= stop_time;
               });
    }

    std::vector<float> getEventsAsVector(float start_time, float stop_time) const {
        std::vector<float> result;
        for (float time: _data) {
            if (time >= start_time && time <= stop_time) {
                result.push_back(time);
            }
        }
        return result;
    }

    template<typename TransformFunc>
    std::vector<float> getEventsAsVector(float start_time, float stop_time, TransformFunc time_transform) const {
        std::vector<float> result;
        for (float time: _data) {
            auto transformed_time = time_transform(time);
            if (transformed_time >= start_time && transformed_time <= stop_time) {
                result.push_back(time);
            }
        }
        return result;
    }

    // ========== Events with EntityIDs ==========

    /**
     * @brief Get events in range with their EntityIDs
     * 
     * @param start_time Start time for the range
     * @param stop_time Stop time for the range
     * @return std::vector<EventWithId> Vector of events with their EntityIDs
     */
    [[nodiscard]] std::vector<EventWithId> getEventsWithIdsInRange(float start_time, float stop_time) const;

    /**
     * @brief Get events in range with their EntityIDs using TimeFrameIndex
     * 
     * @param start_time Start time index for the range
     * @param stop_time Stop time index for the range
     * @return std::vector<EventWithId> Vector of events with their EntityIDs
     */
    [[nodiscard]] std::vector<EventWithId> getEventsWithIdsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const;

    /**
     * @brief Get events in range with their EntityIDs using time frame conversion
     * 
     * @param start_index Start time index in source timeframe
     * @param stop_index Stop time index in source timeframe
     * @param source_time_frame Source timeframe for the indices
     * @param event_time_frame Target timeframe for the events
     * @return std::vector<EventWithId> Vector of events with their EntityIDs
     */
    [[nodiscard]] std::vector<EventWithId> getEventsWithIdsInRange(TimeFrameIndex start_index,
                                                                   TimeFrameIndex stop_index,
                                                                   TimeFrame const * source_time_frame,
                                                                   TimeFrame const * event_time_frame) const;

    // ========== Time Frame ==========

    /**
     * @brief Set the time frame
     * 
     * @param time_frame The time frame to set
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { _time_frame = time_frame; }

    // ===== Identity =====
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
        _identity_data_key = data_key;
        _identity_registry = registry;
    }
    void rebuildAllEntityIds();
    [[nodiscard]] std::vector<EntityId> const & getEntityIds() const { return _entity_ids; }

private:
    std::vector<float> _data{};
    std::shared_ptr<TimeFrame> _time_frame{nullptr};

    // Sort the events in ascending order
    void _sortEvents();

    // Identity
    std::string _identity_data_key;
    EntityRegistry * _identity_registry{nullptr};
    std::vector<EntityId> _entity_ids;

    // ========== Helper Functions for Time Frame Conversion ==========

    /**
     * @brief Convert time range from source timeframe to target timeframe
     * 
     * @param start_index Start index in source timeframe
     * @param stop_index Stop index in source timeframe
     * @param source_time_frame Source timeframe
     * @param target_time_frame Target timeframe
     * @return std::pair<TimeFrameIndex, TimeFrameIndex> Converted start and stop indices
     */
    [[nodiscard]] static std::pair<TimeFrameIndex, TimeFrameIndex> _convertTimeFrameRange(
            TimeFrameIndex const start_index,
            TimeFrameIndex const stop_index,
            TimeFrame const * const source_time_frame,
            TimeFrame const * const target_time_frame);

    /**
     * @brief Get time values from TimeFrameIndex range
     * 
     * @param start_index Start time index
     * @param stop_index Stop time index
     * @return std::pair<float, float> Start and stop time values
     */
    [[nodiscard]] std::pair<float, float> _getTimeRangeFromIndices(
            TimeFrameIndex start_index,
            TimeFrameIndex stop_index) const;
};

#endif//BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
