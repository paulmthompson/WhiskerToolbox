#ifndef EVENTPOINTVISUALIZATION_HPP
#define EVENTPOINTVISUALIZATION_HPP

#include "../SpatialOverlayPlotWidget/Points/GenericPointVisualization.hpp"

#include <vector>

class GroupManager;

/**
 * @brief Event-specific implementation of point visualization for event plot data
 * 
 * This class specializes GenericPointVisualization for event plot data structure,
 * where events are organized as trials (vector<vector<float>>). Each event is
 * identified by a simple index and positioned based on its trial and time.
 * 
 * Note: This class defers OpenGL initialization to avoid context issues.
 */
class EventPointVisualization : public GenericPointVisualization<float, size_t> {
public:
    /**
     * @brief Constructor for event-based point visualization
     * @param data_key The key identifier for this visualization
     * @param event_data Vector of trials, each containing event times
     * @param group_manager Optional group manager for color coding
     * @param defer_opengl_init If true, don't initialize OpenGL resources in constructor
     */
    EventPointVisualization(QString const & data_key,
                           std::vector<std::vector<float>> const & event_data,
                           GroupManager * group_manager = nullptr,
                           bool defer_opengl_init = true);

    /**
     * @brief Destructor
     */
    ~EventPointVisualization() override = default;

    /**
     * @brief Get tooltip text for the currently hovered point
     * @return Formatted tooltip string with trial and event information
     */
    QString getEventTooltipText() const;

    /**
     * @brief Get the trial index for a given point
     * @param point_index The global point index
     * @return Trial index, or -1 if invalid
     */
    int getTrialIndex(size_t point_index) const;

    /**
     * @brief Get the event index within a trial for a given point
     * @param point_index The global point index
     * @return Event index within trial, or -1 if invalid
     */
    int getEventIndexInTrial(size_t point_index) const;

    /**
     * @brief Get the event time for a given point
     * @param point_index The global point index
     * @return Event time, or 0.0f if invalid
     */
    float getEventTime(size_t point_index) const;

protected:
    /**
     * @brief Populate data from event vector inputs
     * @post The spatial index and vertex data will be populated with events
     */
    void populateData() override;

    /**
     * @brief Get the bounding box for the event data
     * @return BoundingBox containing all event points
     */
    BoundingBox getDataBounds() const override;

private:
    std::vector<std::vector<float>> m_event_data;
    
    // Mapping from global point index to trial/event coordinates
    struct EventMapping {
        size_t trial_index;
        size_t event_index_in_trial;
        float event_time;
        float y_coordinate;
    };
    std::vector<EventMapping> m_event_mappings;
};

#endif// EVENTPOINTVISUALIZATION_HPP
