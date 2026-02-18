#ifndef MLCORE_FEATUREVALIDATOR_HPP
#define MLCORE_FEATUREVALIDATOR_HPP

/**
 * @file FeatureValidator.hpp
 * @brief Validates that a TensorData feature matrix is suitable for ML workflows
 *
 * The TensorDesigner/TensorColumnBuilders infrastructure handles the complex work
 * of gathering data from heterogeneous sources, applying transforms, and aligning
 * time bases. This validator checks the ML-specific requirements:
 * - The tensor is non-empty and 2D
 * - Row type is compatible with the label source
 * - Row counts match between features and labels
 * - Time frames are compatible when both features and labels are time-indexed
 *
 * @see ml_library_roadmap.md §3.4.3
 */

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

// Forward declarations
class TensorData;
class RowDescriptor;

namespace MLCore {

// ============================================================================
// Validation result types
// ============================================================================

/**
 * @brief Reason why a feature tensor is incompatible with a label source
 */
enum class RowCompatibility {
    Compatible,           ///< Feature tensor is compatible with label source
    EmptyTensor,          ///< Tensor has no storage or zero rows
    Not2D,                ///< Tensor is not 2-dimensional
    NoColumns,            ///< Tensor has zero columns
    RowTypeMismatch,      ///< e.g., tensor has Interval rows but labels expect TimeFrameIndex
    TimeFrameMismatch,    ///< Feature and label time frames are different objects (different clocks)
    RowCountMismatch,     ///< Number of tensor rows != expected label count
};

/**
 * @brief Human-readable description of a RowCompatibility value
 */
[[nodiscard]] std::string toString(RowCompatibility rc);

// ============================================================================
// Label source descriptors (lightweight — no DataManager dependency)
// ============================================================================

/**
 * @brief Describes a label source based on entity groups of time entities
 *
 * Each group maps to one class label. The total label count is the sum
 * of entity counts across all groups.
 */
struct LabelSourceTimeGroups {
    std::size_t total_label_count{0};   ///< Total entities across all class groups
    bool has_time_frame{false};         ///< Whether the time entities reference a time frame
};

/**
 * @brief Describes a label source based on a DigitalIntervalSeries
 *
 * Binary labeling: frames inside intervals = class 1, outside = class 0.
 */
struct LabelSourceIntervals {
    std::size_t expected_row_count{0};  ///< Number of rows (frames) to label
    bool has_time_frame{false};         ///< Whether the interval series has a time frame
};

/**
 * @brief Describes a label source based on entity groups on data objects
 *
 * Each group contains entities (e.g., lines, points) that map to class labels.
 */
struct LabelSourceEntityGroups {
    std::size_t total_label_count{0};   ///< Total entities across all class groups
};

/**
 * @brief Describes the expected label source for validation
 *
 * This is a lightweight descriptor — it captures only the metadata needed
 * to validate compatibility with a feature tensor, without requiring
 * access to the full DataManager or EntityGroupManager.
 */
using LabelSourceDescriptor = std::variant<
    LabelSourceTimeGroups,
    LabelSourceIntervals,
    LabelSourceEntityGroups>;

// ============================================================================
// Tensor validation (standalone — no label dependency)
// ============================================================================

/**
 * @brief Result of standalone tensor validation
 */
struct TensorValidationResult {
    bool valid{false};
    RowCompatibility reason{RowCompatibility::Compatible};
    std::string message;
};

/**
 * @brief Validate that a TensorData is structurally suitable as an ML feature matrix
 *
 * Checks:
 * - Non-empty (has storage)
 * - 2D (rows × columns)
 * - Has at least one column
 * - Has at least one row
 *
 * Does NOT check label compatibility — use validateFeatureLabelCompatibility() for that.
 *
 * @param features The tensor to validate
 * @return Validation result with reason and human-readable message
 */
[[nodiscard]] TensorValidationResult validateFeatureTensor(TensorData const & features);

// ============================================================================
// Feature-label compatibility validation
// ============================================================================

/**
 * @brief Full validation result including label compatibility
 */
struct FeatureLabelValidationResult {
    bool valid{false};
    RowCompatibility reason{RowCompatibility::Compatible};
    std::string message;
    std::size_t feature_rows{0};
    std::size_t feature_cols{0};
    std::size_t label_count{0};
};

/**
 * @brief Validate that a feature tensor is compatible with a label source
 *
 * Performs all checks from validateFeatureTensor(), plus:
 * - Row count matches the label source's expected count
 * - For time-indexed features with time-indexed labels: verifies compatible row types
 *
 * @param features The feature tensor (built by TensorDesigner)
 * @param label_source Description of the label source
 * @return Detailed validation result
 */
[[nodiscard]] FeatureLabelValidationResult validateFeatureLabelCompatibility(
    TensorData const & features,
    LabelSourceDescriptor const & label_source);

/**
 * @brief Check if a feature tensor contains any NaN or Inf values
 *
 * Scans all columns for non-finite values. Useful as a pre-flight check
 * before conversion, since mlpack cannot handle NaN/Inf.
 *
 * @param features The feature tensor to check
 * @return Number of rows containing at least one NaN or Inf value
 */
[[nodiscard]] std::size_t countNonFiniteRows(TensorData const & features);

/**
 * @brief Get the indices of rows that contain NaN or Inf values
 *
 * @param features The feature tensor to check
 * @return Sorted vector of row indices containing non-finite values
 */
[[nodiscard]] std::vector<std::size_t> findNonFiniteRows(TensorData const & features);

} // namespace MLCore

#endif // MLCORE_FEATUREVALIDATOR_HPP