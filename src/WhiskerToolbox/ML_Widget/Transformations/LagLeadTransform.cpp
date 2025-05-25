#include "LagLeadTransform.hpp"
#include <iostream>
#include <vector>

arma::Mat<double> LagLeadTransform::_applyTransformationLogic(
        arma::Mat<double> const & base_data,// base_data is (num_base_features x num_timestamps)
        WhiskerTransformations::AppliedTransformation const & transform_config,
        std::string & error_message) const {
    auto const * params = std::get_if<WhiskerTransformations::LagLeadParams>(&transform_config.params);
    if (!params) {
        error_message = "LagLeadTransform: Invalid parameters provided.";
        return arma::Mat<double>();
    }

    int min_lag = params->min_lag_steps;  // e.g., -2, -1, 0 (0 means current value, negative means lag)
    int max_lead = params->max_lead_steps;// e.g., 0, 1, 2 (0 means current value, positive means lead)

    std::cout << "Applying lead and lag transform with lead equal to "
              << params->min_lag_steps << " and lag "
              << params->max_lead_steps << std::endl;

    if (min_lag > 0 || max_lead < 0) {
        error_message = "LagLeadTransform: min_lag_steps must be <= 0 and max_lead_steps must be >= 0.";
        return arma::Mat<double>();
    }
    //if (min_lag == 0 && max_lead == 0) {
    //    return base_data;
    //}

    arma::uword n_rows_base = base_data.n_rows;// Number of original features
    arma::uword n_cols_base = base_data.n_cols;// Number of timestamps

    if (n_cols_base == 0) {
        error_message = "LagLeadTransform: Base data is empty (0 timestamps).";
        return arma::Mat<double>();
    }

    // Determine the number of shifted versions we'll create
    // Example: min_lag = -1, max_lead = 1. Shifts: -1, 0, 1. Total = 1 - (-1) + 1 = 3 shifts.
    int num_shifts = max_lead - min_lag + 1;
    if (num_shifts <= 0) {// Should not happen with prior checks, but as a safeguard.
        error_message = "LagLeadTransform: Invalid lag/lead range.";
        return arma::Mat<double>();
    }

    arma::uword n_rows_output = n_rows_base * num_shifts;
    arma::Mat<double> result(n_rows_output, n_cols_base, arma::fill::zeros);


    for (int shift = min_lag; shift <= max_lead; ++shift) {
        // Determine the target row block in the result matrix
        // Example: min_lag = -2. Shifts: -2, -1, 0, 1, 2
        // shift = -2 (lag 2) -> block_idx = 0
        // shift = -1 (lag 1) -> block_idx = 1
        // shift =  0 (current) -> block_idx = 2
        // shift =  1 (lead 1) -> block_idx = 3
        arma::uword current_block_idx = static_cast<arma::uword>(shift - min_lag);
        arma::uword target_row_start = current_block_idx * n_rows_base;

        for (arma::uword ts_idx = 0; ts_idx < n_cols_base; ++ts_idx) {
            long long source_ts_long = static_cast<long long>(ts_idx) - shift;// Shift is subtracted: lag (-s) increases index, lead (+s) decreases index

            if (source_ts_long >= 0 && source_ts_long < static_cast<long long>(n_cols_base)) {
                arma::uword source_ts_idx = static_cast<arma::uword>(source_ts_long);
                result.submat(target_row_start, ts_idx, target_row_start + n_rows_base - 1, ts_idx) =
                        base_data.col(source_ts_idx);
            } else {
                for (arma::uword rr = 0; rr < n_rows_base; rr++) {
                    result.submat(target_row_start + rr, ts_idx, target_row_start + rr, ts_idx) = arma::datum::nan;
                }
            }
        }
    }

    return result;
}

bool LagLeadTransform::isSupported(DM_DataType type) const {
    return type == DM_DataType::Analog ||
           type == DM_DataType::Points ||
           type == DM_DataType::Tensor;
}
