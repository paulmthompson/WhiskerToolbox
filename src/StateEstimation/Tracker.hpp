#ifndef TRACKER_HPP
#define TRACKER_HPP

#include "Assignment/IAssigner.hpp"
#include "Filter/IFilter.hpp"

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace StateEstimation {

/**
 * @brief Abstract interface for extracting feature vectors from a raw data type.
 *
 * This allows the Tracker to remain generic and decoupled from the specifics
 * of any given data type (e.g., Line2D, Point2D). A concrete implementation
 * of this interface will know how to convert a specific DataType into the
 * feature vectors needed by the IFilter and IAssigner.
 *
 * @tparam DataType The raw data type (e.g., CoreGeometry::Line2D).
 */
template<typename DataType>
class IFeatureExtractor {
public:
    virtual ~IFeatureExtractor() = default;

    /**
     * @brief Extracts all features from a data object and returns them in a cache.
     * This is used to populate the feature cache that gets passed to the IAssigner.
     * @param data The raw data object (e.g., Line2D).
     * @return A map of feature names to their values.
     */
    virtual FeatureCache extractFeatures(DataType const & data) const = 0;

    /**
     * @brief Extracts the specific feature vector required for the filter's update step.
     * @param data The raw data object.
     * @return A Measurement struct containing the correctly formatted feature vector.
     */
    virtual Measurement getFilterMeasurement(DataType const & data) const = 0;

    /**
     * @brief Clones the feature extractor.
     * @return A std::unique_ptr to a new instance of the extractor.
     */
    virtual std::unique_ptr<IFeatureExtractor<DataType>> clone() const = 0;
};


// The final output of a smoothing-only operation.
// Maps each GroupId to its full time-series of smoothed states.
using SmoothedResults = std::map<GroupId, std::vector<FilterState>>;
using ProgressCallback = std::function<void(double)>;

/**
 * @brief The central class for orchestrating tracking, assignment, and smoothing.
 *
 * The Tracker manages the lifecycle of tracked objects (groups) and coordinates
 * the filter and assigner components to process data over a range of time frames.
 *
 * @tparam DataType The raw data type being tracked (e.g., CoreGeometry::Line2D).
 */
template<typename DataType>
class Tracker {
public:
    using TimePoint = int64_t;
    using ObjectID = uint64_t;

    // A map from a unique object ID to the data object itself for a single time point.
    using FrameObservations = std::map<ObjectID, DataType>;
    // A map from a time point (frame index) to the observations in that frame.
    using TimeSeriesObservations = std::map<TimePoint, FrameObservations>;
    // A map from an object's unique ID to the group it belongs to.
    using GroupAssignments = std::map<ObjectID, GroupId>;

    /**
     * @brief Holds the complete results of a tracking session.
     */
    struct ProcessResults {
        GroupAssignments assignments;
        SmoothedResults smoothed_states;
    };

    /**
     * @brief Constructs a Tracker with prototype algorithms and a feature extractor.
     *
     * @param filter_prototype A unique_ptr to an IFilter implementation. This will be cloned for each new track.
     * @param assigner A unique_ptr to an IAssigner. Can be nullptr for smoothing/outlier detection.
     * @param feature_extractor A unique_ptr to an IFeatureExtractor for the specified DataType.
     */
    Tracker(std::unique_ptr<IFilter> filter_prototype,
            std::unique_ptr<IAssigner> assigner,
            std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor);

    /**
     * @brief The main entry point to run a tracking/smoothing session.
     *
     * This function iterates through a time series of observations, applying the configured
     * filter and assigner. It returns the final group assignments and smoothed trajectories.
     *
     * @param observations The time-series data to be processed.
     * @param initial_groups The initial ground-truth group assignments.
     * @param progress_callback A function to report progress (0.0 to 1.0).
     * @return The results of the processing, including final assignments and smoothed states.
     */
    ProcessResults process(
            TimeSeriesObservations<DataType> const & observations,
            GroupAssignments const & initial_groups,
            ProgressCallback progress_callback);

private:
    // Internal state for managing an individual tracked group.
    struct TrackedGroupState {
        GroupId group_id;
        std::unique_ptr<IFilter> filter;
        int frames_since_last_seen = 0;
        double confidence = 1.0;

        // For asynchronous smoothing
        std::vector<TimePoint> anchor_frames;
        std::vector<Measurement> anchor_measurements;
        std::vector<FilterState> forward_pass_history;
    };

    // Prototypes for creating new instances
    std::unique_ptr<IFilter> filter_prototype_;
    std::unique_ptr<IAssigner> assigner_;
    std::unique_ptr<IFeatureExtractor<DataType>> feature_extractor_;

    // Map to hold the state of all currently active tracks.
    std::unordered_map<GroupId, TrackedGroupState> active_tracks_;

    // --- Private Helper Methods ---
    // (e.g., for initializing new tracks, handling lost tracks, populating feature cache, etc.)
};

}// namespace StateEstimation

#endif// TRACKER_HPP
