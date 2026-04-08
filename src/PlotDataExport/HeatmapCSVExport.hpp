/**
 * @file HeatmapCSVExport.hpp
 * @brief Export heatmap data to CSV format
 *
 * Provides free functions to serialize heatmap data into CSV strings.
 * Two export modes are supported:
 *
 * **Aggregate** — wide matrix, one row per unit:
 * @code
 * unit_key,-500.0,-490.0,-480.0,...
 * unit_A,0.4,1.2,3.7,...
 * unit_B,0.0,0.3,0.5,...
 * @endcode
 *
 * **Per-trial** — long-form tidy, one row per (unit, trial, bin):
 * @code
 * unit_key,trial_index,bin_center,value
 * unit_A,0,-500.0,0.0
 * unit_A,0,-490.0,1.0
 * unit_A,1,-500.0,0.0
 * unit_B,0,-500.0,0.0
 * @endcode
 *
 * The metadata comment header (lines starting with #) records alignment
 * parameters for reproducibility. Both Python (pd.read_csv(comment='#'))
 * and R (read_csv(comment='#')) handle this natively.
 *
 * ## Widget integration note
 *
 * The per-trial export function takes a self-contained `PerTrialHeatmapInput`
 * struct rather than `RateEstimateWithTrials` directly. This keeps
 * `PlotDataExport` free of the `EventRateEstimation` dependency (which lives
 * under the UI layer). Widgets are responsible for converting
 * `RateEstimateWithTrials` to `PerTrialHeatmapInput` before calling this API.
 *
 * @see CorePlotting::HeatmapRowData for the aggregate source data type
 * @see exportHeatmapToCSV() for aggregate export
 * @see exportHeatmapPerTrialToCSV() for per-trial export
 */

#ifndef PLOT_DATA_EXPORT_HEATMAP_CSV_EXPORT_HPP
#define PLOT_DATA_EXPORT_HEATMAP_CSV_EXPORT_HPP

#include "CorePlotting/Mappers/HeatmapMapper.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace PlotDataExport {

/**
 * @brief Metadata written as comment header in the CSV output
 *
 * These values are recorded for reproducibility. They do not affect
 * the data rows — they are informational only.
 */
struct HeatmapExportMetadata {
    std::string alignment_key;
    double window_size = 0.0;
    std::string scaling_mode;     ///< e.g. "FiringRateHz", "ZScore", "RawCount"
    std::string estimation_method;///< e.g. "Binning", "GaussianKernel", "CausalExponential"
};

/**
 * @brief Per-trial data for one unit, suitable for per-trial CSV export
 *
 * Widgets convert `RateEstimateWithTrials` to this struct before calling
 * `exportHeatmapPerTrialToCSV()`. This keeps `PlotDataExport` independent
 * of the `EventRateEstimation` library.
 *
 * @pre `times.size() == per_trial_values[i].size()` for all trials i.
 */
struct PerTrialHeatmapInput {
    std::string unit_key;
    std::vector<double> times;                        ///< Bin centers (shared across trials)
    std::vector<std::vector<double>> per_trial_values;///< [trial_idx][time_idx]
};

/**
 * @brief Export aggregate heatmap data to a wide-matrix CSV string
 *
 * Produces one row per unit where columns are the time-bin values.
 * The header row contains the bin center values as column names, preceded
 * by "unit_key".
 *
 * Bin centers are reconstructed from the first row's `bin_start` and
 * `bin_width`. If `rows` is empty the output contains only the metadata
 * comment header.
 *
 * @pre `unit_keys.size() == rows.size()`
 * @pre All rows have the same number of bins.
 *
 * @param unit_keys  Names for each unit row (e.g. DataManager key strings)
 * @param rows       Heatmap row data (one element per unit)
 * @param metadata   Optional metadata for the comment header
 * @param delimiter  Column delimiter (default: comma)
 * @return CSV string ready to be written to a file
 *
 * @code
 * auto csv = exportHeatmapToCSV(unit_keys, pipeline_result.rows,
 *                               {"stimulus_onset", 1000.0, "FiringRateHz", "Binning"});
 * @endcode
 */
[[nodiscard]] inline std::string exportHeatmapToCSV(
        std::vector<std::string> const & unit_keys,
        std::vector<CorePlotting::HeatmapRowData> const & rows,
        HeatmapExportMetadata const & metadata = {},
        std::string_view delimiter = ",") {

    assert(unit_keys.size() == rows.size() &&
           "exportHeatmapToCSV: unit_keys and rows must have the same size");

    std::ostringstream out;

    // --- Metadata comment header ---
    out << "# export_type: heatmap_aggregate\n";
    if (!metadata.alignment_key.empty()) {
        out << "# alignment_key: " << metadata.alignment_key << '\n';
    }
    if (metadata.window_size > 0.0) {
        out << "# window_size: " << metadata.window_size << '\n';
    }
    if (!metadata.scaling_mode.empty()) {
        out << "# scaling_mode: " << metadata.scaling_mode << '\n';
    }
    if (!metadata.estimation_method.empty()) {
        out << "# estimation_method: " << metadata.estimation_method << '\n';
    }

    if (rows.empty()) {
        out << "unit_key\n";
        return out.str();
    }

    // Compute bin centers from first row
    std::size_t const num_bins = rows.front().values.size();
    double const bin_start = rows.front().bin_start;
    double const bin_width = rows.front().bin_width;

    // --- CSV header: unit_key, <bin_center_0>, <bin_center_1>, ... ---
    out << "unit_key";
    for (std::size_t i = 0; i < num_bins; ++i) {
        double const center = bin_start + (static_cast<double>(i) + 0.5) * bin_width;
        out << delimiter << center;
    }
    out << '\n';

    // --- Data rows ---
    for (std::size_t unit = 0; unit < rows.size(); ++unit) {
        out << unit_keys[unit];
        for (double const val: rows[unit].values) {
            out << delimiter << val;
        }
        out << '\n';
    }

    return out.str();
}

/**
 * @brief Export per-trial heatmap data to a long-form tidy CSV string
 *
 * Produces one row per (unit, trial, time bin) combination.
 * Columns: `unit_key, trial_index, bin_center, value`.
 *
 * @pre For each entry in @p data, `times.size() == per_trial_values[i].size()`
 *      for all trial indices i.
 *
 * @param data       Per-trial data, one element per unit
 * @param metadata   Optional metadata for the comment header
 * @param delimiter  Column delimiter (default: comma)
 * @return CSV string ready to be written to a file
 *
 * @code
 * PerTrialHeatmapInput input;
 * input.unit_key = "unit_A";
 * input.times = rate_with_trials.estimate.times;
 * input.per_trial_values = rate_with_trials.trials.per_trial_values;
 * auto csv = exportHeatmapPerTrialToCSV({input}, metadata);
 * @endcode
 */
[[nodiscard]] inline std::string exportHeatmapPerTrialToCSV(
        std::vector<PerTrialHeatmapInput> const & data,
        HeatmapExportMetadata const & metadata = {},
        std::string_view delimiter = ",") {

    std::ostringstream out;

    // --- Metadata comment header ---
    out << "# export_type: heatmap_per_trial\n";
    if (!metadata.alignment_key.empty()) {
        out << "# alignment_key: " << metadata.alignment_key << '\n';
    }
    if (metadata.window_size > 0.0) {
        out << "# window_size: " << metadata.window_size << '\n';
    }
    if (!metadata.scaling_mode.empty()) {
        out << "# scaling_mode: " << metadata.scaling_mode << '\n';
    }
    if (!metadata.estimation_method.empty()) {
        out << "# estimation_method: " << metadata.estimation_method << '\n';
    }

    // --- CSV header ---
    out << "unit_key" << delimiter
        << "trial_index" << delimiter
        << "bin_center" << delimiter
        << "value" << '\n';

    // --- Data rows ---
    for (auto const & unit: data) {
        for (std::size_t trial = 0; trial < unit.per_trial_values.size(); ++trial) {
            auto const & trial_vals = unit.per_trial_values[trial];
            assert(trial_vals.size() == unit.times.size() &&
                   "PerTrialHeatmapInput: times and per_trial_values[trial] must have the same size");

            for (std::size_t bin = 0; bin < trial_vals.size(); ++bin) {
                out << unit.unit_key << delimiter
                    << trial << delimiter
                    << unit.times[bin] << delimiter
                    << trial_vals[bin] << '\n';
            }
        }
    }

    return out.str();
}

}// namespace PlotDataExport

#endif// PLOT_DATA_EXPORT_HEATMAP_CSV_EXPORT_HPP
