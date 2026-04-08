/**
 * @file HistogramCSVExport.hpp
 * @brief Export PSTH/histogram data to CSV format
 *
 * Provides free functions to serialize histogram data (CorePlotting::HistogramData)
 * into long-form tidy CSV strings. Designed for PSTH (Peri-Stimulus Time Histogram)
 * export but usable for any histogram-type data.
 *
 * CSV format (one row per bin per series):
 * @code
 * bin_center,event_key,value
 * -500.0,spikes,0.0
 * -490.0,spikes,2.4
 * -480.0,spikes,1.8
 * -500.0,licks,0.0
 * -490.0,licks,0.5
 * @endcode
 *
 * The metadata comment header (lines starting with #) records alignment
 * parameters for reproducibility. Both Python (pd.read_csv(comment='#'))
 * and R (read_csv(comment='#')) handle this natively.
 *
 * @see CorePlotting::HistogramData for the source data type
 * @see exportHistogramToCSV() for the primary export function
 */

#ifndef PLOT_DATA_EXPORT_HISTOGRAM_CSV_EXPORT_HPP
#define PLOT_DATA_EXPORT_HISTOGRAM_CSV_EXPORT_HPP

#include "CorePlotting/DataTypes/HistogramData.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace PlotDataExport {

/**
 * @brief Input descriptor for one histogram series in a PSTH export
 *
 * Each HistogramSeriesInput represents one named event series (e.g., "spikes",
 * "licks") with its pre-computed histogram data.
 */
struct HistogramSeriesInput {
    std::string event_key;
    CorePlotting::HistogramData const * data;
};

/**
 * @brief Metadata written as comment header in the CSV output
 *
 * These values are recorded for reproducibility. They do not affect
 * the data rows — they are informational only.
 */
struct HistogramExportMetadata {
    std::string alignment_key;
    double window_size = 0.0;
    std::string scaling_mode;     ///< e.g. "FiringRateHz", "ZScore", "RawCount"
    std::string estimation_method;///< e.g. "Binning", "GaussianKernel", "CausalExponential"
};

/**
 * @brief Export histogram/PSTH data to a CSV string
 *
 * Produces a long-form tidy CSV with one row per (bin, series) pair.
 * Multiple series are interleaved, distinguished by the event_key column.
 *
 * @pre All entries in @p series must have non-null data pointers.
 *
 * @param series    Vector of histogram series to export
 * @param metadata  Optional metadata for the comment header
 * @param delimiter Column delimiter (default: comma)
 * @return CSV string ready to be written to a file
 *
 * @code
 * HistogramSeriesInput input{"spikes", &histogram_data};
 * auto csv = exportHistogramToCSV({input}, {"stimulus_onset", 1000.0, "FiringRateHz", "Binning"});
 * @endcode
 */
[[nodiscard]] inline std::string exportHistogramToCSV(
        std::vector<HistogramSeriesInput> const & series,
        HistogramExportMetadata const & metadata = {},
        std::string_view delimiter = ",") {

    std::ostringstream out;

    // --- Metadata comment header ---
    out << "# export_type: psth\n";
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
    out << "bin_center" << delimiter
        << "event_key" << delimiter
        << "value" << '\n';

    // --- Data rows ---
    for (auto const & s: series) {
        assert(s.data != nullptr && "HistogramSeriesInput::data must not be null");

        auto const & hist = *s.data;

        for (std::size_t i = 0; i < hist.numBins(); ++i) {
            out << hist.binCenter(i) << delimiter
                << s.event_key << delimiter
                << hist.counts[i] << '\n';
        }
    }

    return out.str();
}

}// namespace PlotDataExport

#endif// PLOT_DATA_EXPORT_HISTOGRAM_CSV_EXPORT_HPP
