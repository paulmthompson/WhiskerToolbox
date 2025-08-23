#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"

#include <ranges>
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

        // Convert the time index from source timeframe to target timeframe
        // 1. Get the time value from the source timeframe
        auto start_time_value = source_time_frame->getTimeAtIndex(start_index);
        auto stop_time_value = source_time_frame->getTimeAtIndex(stop_index);

        auto target_start_index = event_time_frame->getIndexAtTime(static_cast<float>(start_time_value), false);
        auto target_stop_index = event_time_frame->getIndexAtTime(static_cast<float>(stop_time_value));

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
        for (float time : _data) {
            if (time >= start_time && time <= stop_time) {
                result.push_back(time);
            }
        }
        return result;
    }

    template<typename TransformFunc>
    std::vector<float> getEventsAsVector(float start_time, float stop_time, TransformFunc time_transform) const {
        std::vector<float> result;
        for (float time : _data) {
            auto transformed_time = time_transform(time);
            if (transformed_time >= start_time && transformed_time <= stop_time) {
                result.push_back(time);
            }
        }
        return result;
    }

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
    std::shared_ptr<TimeFrame> _time_frame {nullptr};

    // Sort the events in ascending order
    void _sortEvents();

    // Identity
    std::string _identity_data_key;
    EntityRegistry * _identity_registry {nullptr};
    std::vector<EntityId> _entity_ids;
};

#endif//BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
