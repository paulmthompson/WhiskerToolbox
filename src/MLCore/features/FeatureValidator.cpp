/**
 * @file FeatureValidator.cpp
 * @brief Implementation of feature tensor and feature-label compatibility validation
 */

#include "FeatureValidator.hpp"

#include "DataManager/Tensors/RowDescriptor.hpp"
#include "DataManager/Tensors/TensorData.hpp"

#include <cmath>
#include <sstream>

namespace MLCore {

// ============================================================================
// toString
// ============================================================================

std::string toString(RowCompatibility rc) {
    switch (rc) {
        case RowCompatibility::Compatible:
            return "Compatible";
        case RowCompatibility::EmptyTensor:
            return "Empty tensor (no storage or zero rows)";
        case RowCompatibility::Not2D:
            return "Tensor is not 2-dimensional";
        case RowCompatibility::NoColumns:
            return "Tensor has zero columns";
        case RowCompatibility::RowTypeMismatch:
            return "Row type mismatch between features and labels";
        case RowCompatibility::TimeFrameMismatch:
            return "Time frame mismatch between features and labels";
        case RowCompatibility::RowCountMismatch:
            return "Row count mismatch between features and labels";
    }
    return "Unknown";
}

// ============================================================================
// Standalone tensor validation
// ============================================================================

TensorValidationResult validateFeatureTensor(TensorData const & features) {
    if (features.isEmpty()) {
        return {false, RowCompatibility::EmptyTensor, "Feature tensor is empty (no storage)"};
    }

    if (features.ndim() != 2) {
        std::ostringstream oss;
        oss << "Feature tensor must be 2D, but has " << features.ndim() << " dimensions";
        return {false, RowCompatibility::Not2D, oss.str()};
    }

    if (features.numColumns() == 0) {
        return {false, RowCompatibility::NoColumns, "Feature tensor has zero columns"};
    }

    if (features.numRows() == 0) {
        return {false, RowCompatibility::EmptyTensor, "Feature tensor has zero rows"};
    }

    return {true, RowCompatibility::Compatible, "Feature tensor is valid"};
}

// ============================================================================
// NaN/Inf scanning helpers
// ============================================================================

namespace {

/**
 * @brief Check if any element in a row is non-finite
 *
 * Iterates all columns and checks the value at the given row index.
 * Materializes column data via getColumn() which handles lazy backends.
 */
bool isRowNonFinite(
        std::vector<std::vector<float>> const & columns,
        std::size_t row) {
    for (auto const & col: columns) {
        if (!std::isfinite(col[row])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Materialize all columns from a TensorData into a vector-of-vectors
 *
 * This avoids repeated getColumn calls in the scanning loops.
 */
std::vector<std::vector<float>> materializeColumns(TensorData const & features) {
    auto const num_cols = features.numColumns();
    std::vector<std::vector<float>> columns;
    columns.reserve(num_cols);
    for (std::size_t c = 0; c < num_cols; ++c) {
        columns.push_back(features.getColumn(c));
    }
    return columns;
}

}// anonymous namespace

std::size_t countNonFiniteRows(TensorData const & features) {
    if (features.isEmpty() || features.numRows() == 0) {
        return 0;
    }

    auto const columns = materializeColumns(features);
    auto const num_rows = features.numRows();
    std::size_t count = 0;

    for (std::size_t r = 0; r < num_rows; ++r) {
        if (isRowNonFinite(columns, r)) {
            ++count;
        }
    }

    return count;
}

std::vector<std::size_t> findNonFiniteRows(TensorData const & features) {
    if (features.isEmpty() || features.numRows() == 0) {
        return {};
    }

    auto const columns = materializeColumns(features);
    auto const num_rows = features.numRows();
    std::vector<std::size_t> bad_rows;

    for (std::size_t r = 0; r < num_rows; ++r) {
        if (isRowNonFinite(columns, r)) {
            bad_rows.push_back(r);
        }
    }

    return bad_rows;
}

// ============================================================================
// Feature-label compatibility validation
// ============================================================================

FeatureLabelValidationResult validateFeatureLabelCompatibility(
        TensorData const & features,
        LabelSourceDescriptor const & label_source) {
    // First run standalone tensor validation
    auto tensor_result = validateFeatureTensor(features);
    if (!tensor_result.valid) {
        return {
                false,
                tensor_result.reason,
                tensor_result.message,
                features.isEmpty() ? 0 : features.numRows(),
                features.isEmpty() ? 0 : features.numColumns(),
                0};
    }

    auto const feature_rows = features.numRows();
    auto const feature_cols = features.numColumns();

    // Get the expected label count from the descriptor
    auto const label_count = std::visit([](auto const & src) -> std::size_t {
        using T = std::decay_t<decltype(src)>;
        if constexpr (std::is_same_v<T, LabelSourceIntervals>) {
            return src.expected_row_count;
        } else {
            return src.total_label_count;
        }
    },
                                        label_source);

    // Check row type compatibility
    auto const row_type = features.rowType();

    // For time-indexed features, verify label source is compatible
    auto const compatibility = std::visit([&](auto const & src) -> RowCompatibility {
        using T = std::decay_t<decltype(src)>;

        if constexpr (std::is_same_v<T, LabelSourceTimeGroups>) {
            // TimeGroups labels work with TimeFrameIndex rows
            if (row_type == RowType::Interval) {
                return RowCompatibility::RowTypeMismatch;
            }
        } else if constexpr (std::is_same_v<T, LabelSourceIntervals>) {
            // Interval labels work with TimeFrameIndex rows (frame-by-frame labeling)
            // and with Ordinal rows
            if (row_type == RowType::Interval) {
                return RowCompatibility::RowTypeMismatch;
            }
        } else if constexpr (std::is_same_v<T, LabelSourceEntityGroups>) {
            // Entity group labels work with any row type — flexible matching
            // (the entity groups might contain entities at intervals, time frames, etc.)
        }

        return RowCompatibility::Compatible;
    },
                                          label_source);

    if (compatibility != RowCompatibility::Compatible) {
        std::ostringstream oss;
        oss << "Row type mismatch: feature tensor has "
            << (row_type == RowType::TimeFrameIndex ? "TimeFrameIndex"
                : row_type == RowType::Interval     ? "Interval"
                                                    : "Ordinal")
            << " rows, which is incompatible with the label source";
        return {false, compatibility, oss.str(), feature_rows, feature_cols, label_count};
    }

    // Check row count matches label count
    if (feature_rows != label_count) {
        std::ostringstream oss;
        oss << "Row count mismatch: feature tensor has " << feature_rows
            << " rows but label source expects " << label_count << " labels";
        return {false, RowCompatibility::RowCountMismatch, oss.str(),
                feature_rows, feature_cols, label_count};
    }

    return {true, RowCompatibility::Compatible, "Features and labels are compatible",
            feature_rows, feature_cols, label_count};
}

}// namespace MLCore