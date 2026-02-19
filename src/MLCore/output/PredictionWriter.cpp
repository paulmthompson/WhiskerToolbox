/**
 * @file PredictionWriter.cpp
 * @brief Implementation of prediction output to DataManager and entity groups
 */

#include "PredictionWriter.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace MLCore {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

/**
 * @brief Validate common preconditions for prediction writing
 */
void validatePredictionOutput(
        PredictionOutput const & output,
        std::vector<std::string> const & class_names) {
    if (output.class_predictions.n_elem != output.prediction_times.size()) {
        throw std::invalid_argument(
                "PredictionWriter: class_predictions length (" + std::to_string(output.class_predictions.n_elem) + ") != prediction_times length (" + std::to_string(output.prediction_times.size()) + ")");
    }

    if (output.prediction_times.empty()) {
        throw std::invalid_argument(
                "PredictionWriter: prediction_times is empty");
    }

    if (class_names.empty()) {
        throw std::invalid_argument(
                "PredictionWriter: class_names is empty");
    }

    // Check that all predictions are within class_names range
    auto max_label = output.class_predictions.max();
    if (max_label >= class_names.size()) {
        throw std::invalid_argument(
                "PredictionWriter: prediction label " + std::to_string(max_label) + " exceeds class_names size (" + std::to_string(class_names.size()) + ")");
    }
}

/**
 * @brief Build per-class interval series from sorted prediction times
 *
 * Scans prediction_times (assumed sorted ascending) for contiguous runs
 * where predictions[j] == class_index. Adjacent time indices differing
 * by 1 are merged into a single interval.
 */
std::shared_ptr<DigitalIntervalSeries> buildIntervalsForClass(
        arma::Row<std::size_t> const & predictions,
        std::vector<TimeFrameIndex> const & times,
        std::size_t class_index) {
    auto series = std::make_shared<DigitalIntervalSeries>();

    std::optional<int64_t> interval_start;
    std::optional<int64_t> interval_end;

    for (std::size_t j = 0; j < predictions.n_elem; ++j) {
        if (predictions[j] != class_index) {
            // Flush any open interval
            if (interval_start.has_value()) {
                series->addEvent(Interval{*interval_start, *interval_end});
                interval_start.reset();
                interval_end.reset();
            }
            continue;
        }

        int64_t t = times[j].getValue();

        if (!interval_start.has_value()) {
            // Start a new interval
            interval_start = t;
            interval_end = t;
        } else if (t == *interval_end + 1) {
            // Extend current interval (contiguous)
            interval_end = t;
        } else {
            // Gap — flush and start new
            series->addEvent(Interval{*interval_start, *interval_end});
            interval_start = t;
            interval_end = t;
        }
    }

    // Flush final interval
    if (interval_start.has_value()) {
        series->addEvent(Interval{*interval_start, *interval_end});
    }

    return series;
}

/**
 * @brief Determine the number of classes from class_names
 */
std::size_t determineNumClasses(
        std::vector<std::string> const & class_names) {
    // Use class_names size as the authoritative class count
    // (there may be classes with zero predictions)
    return class_names.size();
}

}// anonymous namespace

// ============================================================================
// writePredictionsAsIntervals
// ============================================================================

std::vector<std::string> writePredictionsAsIntervals(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config) {
    validatePredictionOutput(output, class_names);

    std::size_t const num_classes = determineNumClasses(class_names);
    std::vector<std::string> keys;
    keys.reserve(num_classes);

    for (std::size_t c = 0; c < num_classes; ++c) {
        auto intervals = buildIntervalsForClass(
                output.class_predictions, output.prediction_times, c);

        std::string key = config.output_prefix + class_names[c];
        dm.setData<DigitalIntervalSeries>(key, intervals, TimeKey(config.time_key_str));
        keys.push_back(std::move(key));
    }

    return keys;
}

// ============================================================================
// writeProbabilitiesAsAnalog
// ============================================================================

std::vector<std::string> writeProbabilitiesAsAnalog(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config) {
    if (!output.class_probabilities.has_value()) {
        return {};
    }

    validatePredictionOutput(output, class_names);

    arma::mat const & probs = *output.class_probabilities;
    std::size_t const num_classes = class_names.size();
    std::size_t const num_obs = output.prediction_times.size();

    // Probabilities matrix: num_classes × num_observations (mlpack convention)
    if (probs.n_rows < num_classes || probs.n_cols != num_obs) {
        throw std::invalid_argument(
                "PredictionWriter: class_probabilities dimensions (" + std::to_string(probs.n_rows) + "×" + std::to_string(probs.n_cols) + ") incompatible with " + std::to_string(num_classes) + " classes and " + std::to_string(num_obs) + " observations");
    }

    std::vector<std::string> keys;
    keys.reserve(num_classes);

    for (std::size_t c = 0; c < num_classes; ++c) {
        // Extract probability row for this class
        std::vector<float> values(num_obs);
        for (std::size_t j = 0; j < num_obs; ++j) {
            values[j] = static_cast<float>(probs(c, j));
        }

        // Copy time indices
        std::vector<TimeFrameIndex> times = output.prediction_times;

        auto analog = std::make_shared<AnalogTimeSeries>(std::move(values), std::move(times));

        std::string key = config.output_prefix + "prob_" + class_names[c];
        dm.setData<AnalogTimeSeries>(key, analog, TimeKey(config.time_key_str));
        keys.push_back(std::move(key));
    }

    return keys;
}

// ============================================================================
// writeTimePredictionsToGroups
// ============================================================================

std::vector<GroupId> writeTimePredictionsToGroups(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config) {
    validatePredictionOutput(output, class_names);

    auto * groups = dm.getEntityGroupManager();
    if (!groups) {
        throw std::invalid_argument(
                "PredictionWriter: DataManager has no EntityGroupManager");
    }

    std::size_t const num_classes = determineNumClasses(class_names);

    // Bin observations by class
    std::vector<std::vector<std::size_t>> class_obs(num_classes);
    for (std::size_t j = 0; j < output.class_predictions.n_elem; ++j) {
        std::size_t label = output.class_predictions[j];
        class_obs[label].push_back(j);
    }

    std::vector<GroupId> group_ids;
    group_ids.reserve(num_classes);

    for (std::size_t c = 0; c < num_classes; ++c) {
        // Create putative group
        std::string group_name = config.output_prefix + class_names[c];
        GroupId gid = groups->createGroup(group_name,
                                          "ML prediction output for class: " + class_names[c]);

        // Register TimeEntities and add to group
        std::vector<EntityId> entity_ids;
        entity_ids.reserve(class_obs[c].size());

        for (std::size_t obs_idx: class_obs[c]) {
            EntityId eid = dm.ensureTimeEntityId(TimeKey(config.time_key_str), output.prediction_times[obs_idx]);
            entity_ids.push_back(eid);
        }

        groups->addEntitiesToGroup(gid, entity_ids);
        group_ids.push_back(gid);
    }

    groups->notifyGroupsChanged();

    return group_ids;
}

// ============================================================================
// writeClusterAssignmentsToGroups
// ============================================================================

std::vector<GroupId> writeClusterAssignmentsToGroups(
        EntityGroupManager & groups,
        arma::Row<std::size_t> const & assignments,
        std::span<EntityId const> entity_ids,
        std::string const & group_prefix) {
    if (assignments.n_elem != entity_ids.size()) {
        throw std::invalid_argument(
                "PredictionWriter: assignments length (" + std::to_string(assignments.n_elem) + ") != entity_ids length (" + std::to_string(entity_ids.size()) + ")");
    }

    if (entity_ids.empty()) {
        return {};
    }

    // Determine number of clusters
    std::size_t const num_clusters = static_cast<std::size_t>(assignments.max()) + 1;

    // Bin entities by cluster
    std::vector<std::vector<EntityId>> cluster_entities(num_clusters);
    for (std::size_t j = 0; j < assignments.n_elem; ++j) {
        cluster_entities[assignments[j]].push_back(entity_ids[j]);
    }

    std::vector<GroupId> group_ids;
    group_ids.reserve(num_clusters);

    for (std::size_t k = 0; k < num_clusters; ++k) {
        std::string cluster_name = group_prefix + std::to_string(k);
        GroupId gid = groups.createGroup(cluster_name,
                                         "Cluster assignment from unsupervised ML");

        groups.addEntitiesToGroup(gid, cluster_entities[k]);
        group_ids.push_back(gid);
    }

    groups.notifyGroupsChanged();

    return group_ids;
}

// ============================================================================
// writePredictions (unified entry point)
// ============================================================================

PredictionWriterResult writePredictions(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config) {
    validatePredictionOutput(output, class_names);

    PredictionWriterResult result;
    result.class_names = class_names;

    // 1. Write interval series
    if (config.write_intervals) {
        result.interval_keys = writePredictionsAsIntervals(dm, output, class_names, config);
    }

    // 2. Write probability analog series
    if (config.write_probabilities && output.class_probabilities.has_value()) {
        result.probability_keys = writeProbabilitiesAsAnalog(dm, output, class_names, config);
    }

    // 3. Write putative groups
    if (config.write_to_putative_groups) {
        result.putative_group_ids = writeTimePredictionsToGroups(dm, output, class_names, config);
    }

    return result;
}

}// namespace MLCore