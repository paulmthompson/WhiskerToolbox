#ifndef MLCORE_PREDICTIONWRITER_HPP
#define MLCORE_PREDICTIONWRITER_HPP

/**
 * @file PredictionWriter.hpp
 * @brief Writes ML prediction output to DataManager and Entity system
 *
 * Implements the **putative group pattern** established by LineKalmanGrouping:
 * predictions are written to new groups prefixed with a configurable string
 * (default "Predicted:"), preserving the original label groups for user triage.
 *
 * Output formats:
 * - **DigitalIntervalSeries**: Contiguous prediction regions per class
 * - **AnalogTimeSeries**: Per-class probability curves
 * - **Entity groups**: Putative groups for classified entities (TimeEntity or data entities)
 * - **Cluster groups**: Entity group assignments for unsupervised clustering output
 *
 * The writer never modifies source label groups. Re-running a model creates
 * fresh putative groups without destroying previous labels or predictions.
 */

#include <armadillo>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

class DataManager;
class EntityGroupManager;
class EntityRegistry;
struct TimeFrameIndex;
struct EntityId;
class TimeKey;
using GroupId = std::uint64_t;

namespace MLCore {

// ============================================================================
// Data types
// ============================================================================

/**
 * @brief Encapsulates the output of a classification or clustering model
 *
 * Holds class predictions, optional probability estimates, and the
 * time indices that each prediction corresponds to.
 */
struct PredictionOutput {
    /**
     * @brief Per-observation class predictions (0-indexed class labels)
     *
     * Size = number of observations.
     */
    arma::Row<std::size_t> class_predictions;

    /**
     * @brief Optional probability estimates per class
     *
     * Layout: num_classes × num_observations (mlpack convention).
     * Column j holds probability distribution over classes for observation j.
     * Absent if the model does not support probability output.
     */
    std::optional<arma::mat> class_probabilities;

    /**
     * @brief Time indices corresponding to each prediction
     *
     * prediction_times[j] is the TimeFrameIndex for observation j.
     * Must have the same length as class_predictions.n_elem.
     */
    std::vector<TimeFrameIndex> prediction_times;
};

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration for how predictions are written to the DataManager
 */
struct PredictionWriterConfig {
    /**
     * @brief Prefix for output data keys and group names
     *
     * Follows the putative group pattern: "Predicted:Contact", "Predicted:No Contact".
     */
    std::string output_prefix = "Predicted:";

    /**
     * @brief Whether to write DigitalIntervalSeries output (one per class)
     *
     * Converts contiguous runs of identical predictions into intervals.
     * Output keys: "{output_prefix}{class_name}"
     */
    bool write_intervals = true;

    /**
     * @brief Whether to write per-class probability AnalogTimeSeries
     *
     * Only effective if PredictionOutput::class_probabilities is present.
     * Output keys: "{output_prefix}prob_{class_name}"
     */
    bool write_probabilities = true;

    /**
     * @brief Whether to create putative entity groups for TimeEntity predictions
     *
     * Creates groups named "{output_prefix}{class_name}" containing TimeEntity IDs.
     */
    bool write_to_putative_groups = true;

    /**
     * @brief Time key string for the clock that prediction_times are expressed in
     *
     * Used to register TimeEntities and set time keys on output data objects.
     * Will be converted to a TimeKey when writing to DataManager.
     */
    std::string time_key_str = "time";
};

// ============================================================================
// Result type
// ============================================================================

/**
 * @brief Summary of what the PredictionWriter produced
 */
struct PredictionWriterResult {
    /**
     * @brief DataManager keys of created DigitalIntervalSeries (one per class)
     *
     * Empty if write_intervals was false.
     */
    std::vector<std::string> interval_keys;

    /**
     * @brief DataManager keys of created AnalogTimeSeries (one per class)
     *
     * Empty if write_probabilities was false or no probabilities available.
     */
    std::vector<std::string> probability_keys;

    /**
     * @brief GroupIds of created putative groups (one per class)
     *
     * Empty if write_to_putative_groups was false.
     */
    std::vector<GroupId> putative_group_ids;

    /**
     * @brief Class names corresponding to the output (same order as above vectors)
     */
    std::vector<std::string> class_names;
};

// ============================================================================
// Primary writer function
// ============================================================================

/**
 * @brief Write all configured prediction outputs in one call
 *
 * This is the main entry point. It:
 * 1. Converts class predictions → DigitalIntervalSeries per class (if enabled)
 * 2. Converts class probabilities → AnalogTimeSeries per class (if enabled)
 * 3. Creates putative entity groups with TimeEntity members (if enabled)
 *
 * @param dm DataManager to write output data into
 * @param output The model's prediction output
 * @param class_names Human-readable names for each class (size = num_classes)
 * @param config Writer configuration
 * @return Summary of all created outputs
 *
 * @pre output.class_predictions.n_elem == output.prediction_times.size()
 * @pre class_names.size() >= max(output.class_predictions) + 1
 * @throws std::invalid_argument if preconditions are violated
 */
[[nodiscard]] PredictionWriterResult writePredictions(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config = {});

// ============================================================================
// Individual writer functions (lower-level)
// ============================================================================

/**
 * @brief Write class predictions as DigitalIntervalSeries
 *
 * For each class, scans prediction_times for contiguous runs where that class
 * was predicted, and creates an interval for each run. Each class produces
 * one DigitalIntervalSeries stored under "{config.output_prefix}{class_name}".
 *
 * @param dm DataManager to write into
 * @param output The prediction output (uses class_predictions and prediction_times)
 * @param class_names Names for each class
 * @param config Writer configuration
 * @return DataManager keys of created interval series (one per class)
 *
 * @note prediction_times must be sorted in ascending order for correct interval merging
 */
[[nodiscard]] std::vector<std::string> writePredictionsAsIntervals(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config = {});

/**
 * @brief Write per-class probabilities as AnalogTimeSeries
 *
 * For each class, extracts the probability row from output.class_probabilities
 * and creates an AnalogTimeSeries with the same time indices as prediction_times.
 * Output keys: "{config.output_prefix}prob_{class_name}"
 *
 * @param dm DataManager to write into
 * @param output The prediction output (uses class_probabilities and prediction_times)
 * @param class_names Names for each class
 * @param config Writer configuration
 * @return DataManager keys of created analog series (one per class), empty if no probabilities
 */
[[nodiscard]] std::vector<std::string> writeProbabilitiesAsAnalog(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config = {});

/**
 * @brief Write time-entity predictions to putative groups
 *
 * For each class, creates a group named "{config.output_prefix}{class_name}"
 * and populates it with TimeEntity IDs corresponding to prediction_times
 * where that class was predicted.
 *
 * @param dm DataManager (used for TimeEntity registration via ensureTimeEntityId)
 * @param output The prediction output
 * @param class_names Names for each class
 * @param config Writer configuration
 * @return GroupIds of created groups (one per class)
 */
[[nodiscard]] std::vector<GroupId> writeTimePredictionsToGroups(
        DataManager & dm,
        PredictionOutput const & output,
        std::vector<std::string> const & class_names,
        PredictionWriterConfig const & config = {});

/**
 * @brief Write cluster assignments to putative entity groups
 *
 * For unsupervised clustering output. Creates groups named
 * "{group_prefix}{cluster_index}" and assigns the corresponding entities.
 *
 * @param groups EntityGroupManager to create groups in
 * @param assignments Per-observation cluster indices
 * @param entity_ids Entity IDs corresponding to each observation
 * @param group_prefix Prefix for group names (default: "Cluster:")
 * @return GroupIds of created groups (one per cluster)
 *
 * @pre assignments.n_elem == entity_ids.size()
 * @throws std::invalid_argument if sizes don't match
 */
[[nodiscard]] std::vector<GroupId> writeClusterAssignmentsToGroups(
        EntityGroupManager & groups,
        arma::Row<std::size_t> const & assignments,
        std::span<EntityId const> entity_ids,
        std::string const & group_prefix = "Cluster:");

}// namespace MLCore

#endif// MLCORE_PREDICTIONWRITER_HPP