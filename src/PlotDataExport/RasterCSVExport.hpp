/**
 * @file RasterCSVExport.hpp
 * @brief Export raster/event plot data to CSV format
 *
 * Provides free functions to serialize trial-aligned event data from
 * GatherResult<DigitalEventSeries> into long-form tidy CSV strings.
 *
 * CSV format (one row per event):
 * @code
 * trial_index,event_key,relative_time
 * 0,spikes,-245.3
 * 0,spikes,-102.1
 * 0,spikes,55.7
 * 1,spikes,-312.0
 * @endcode
 *
 * The metadata comment header (lines starting with #) records alignment
 * parameters for reproducibility. Both Python (pd.read_csv(comment='#'))
 * and R (read_csv(comment='#')) handle this natively.
 *
 * @see GatherResult for the trial-aligned view container
 * @see DigitalEventSeries for the source event data type
 */

#ifndef PLOT_DATA_EXPORT_RASTER_CSV_EXPORT_HPP
#define PLOT_DATA_EXPORT_RASTER_CSV_EXPORT_HPP

#include "GatherResult/GatherResult.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace PlotDataExport {

/**
 * @brief Input descriptor for one event series in a raster export
 *
 * Each RasterSeriesInput represents one named event series (e.g., "spikes",
 * "licks") with its trial-aligned GatherResult and the TimeFrame needed
 * to convert TimeFrameIndex values to absolute time.
 */
struct RasterSeriesInput {
    std::string event_key;
    GatherResult<DigitalEventSeries> const * gathered;
    TimeFrame const * time_frame;
};

/**
 * @brief Metadata written as comment header in the CSV output
 *
 * These values are recorded for reproducibility. They do not affect
 * the data rows — they are informational only.
 */
struct RasterExportMetadata {
    std::string alignment_key;
    double window_size = 0.0;
};

/**
 * @brief Export raster/event plot data to a CSV string
 *
 * Produces a long-form tidy CSV with one row per event occurrence.
 * Multiple event series are interleaved, distinguished by the event_key column.
 *
 * Relative time is computed as:
 *   time_frame->getTimeAtIndex(event.time()) - gathered.alignmentTimeAt(trial)
 *
 * @pre All entries in @p series must have non-null gathered and time_frame pointers.
 *
 * @param series      Vector of event series to export
 * @param metadata    Optional metadata for the comment header
 * @param delimiter   Column delimiter (default: comma)
 * @return CSV string ready to be written to a file
 *
 * @code
 * auto gathered = gather(spikes, trials);
 * RasterSeriesInput input{"spikes", &gathered, time_frame.get()};
 * auto csv = exportRasterToCSV({input}, {"stimulus_onset", 1000.0});
 * @endcode
 */
[[nodiscard]] inline std::string exportRasterToCSV(
        std::vector<RasterSeriesInput> const & series,
        RasterExportMetadata const & metadata = {},
        std::string_view delimiter = ",") {

    std::ostringstream out;

    // --- Metadata comment header ---
    out << "# export_type: raster\n";
    if (!metadata.alignment_key.empty()) {
        out << "# alignment_key: " << metadata.alignment_key << '\n';
    }
    if (metadata.window_size > 0.0) {
        out << "# window_size: " << metadata.window_size << '\n';
    }

    // --- CSV header ---
    out << "trial_index" << delimiter
        << "event_key" << delimiter
        << "relative_time" << '\n';

    // --- Data rows ---
    for (auto const & s: series) {
        assert(s.gathered != nullptr && "RasterSeriesInput::gathered must not be null");
        assert(s.time_frame != nullptr && "RasterSeriesInput::time_frame must not be null");

        auto const & gathered = *s.gathered;

        for (std::size_t trial = 0; trial < gathered.size(); ++trial) {
            auto const & trial_view = gathered[trial];
            if (!trial_view) {
                continue;
            }

            auto const alignment_time = gathered.alignmentTimeAt(trial);

            for (auto const & event: trial_view->view()) {
                auto const abs_time = static_cast<int64_t>(
                        s.time_frame->getTimeAtIndex(event.time()));
                auto const relative_time = static_cast<double>(abs_time - alignment_time);

                out << trial << delimiter
                    << s.event_key << delimiter
                    << relative_time << '\n';
            }
        }
    }

    return out.str();
}

}// namespace PlotDataExport

#endif// PLOT_DATA_EXPORT_RASTER_CSV_EXPORT_HPP
