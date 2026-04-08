/**
 * @file test_HistogramCSVExport.cpp
 * @brief Unit tests for HistogramCSVExport (PSTH/histogram CSV export)
 *
 * Tests cover: empty inputs, single/multiple series, zero-count bins,
 * custom delimiters, and the metadata comment header.
 *
 * Construction pattern: build CorePlotting::HistogramData directly by setting
 * bin_start, bin_width, and counts[], then pass to exportHistogramToCSV().
 *
 * binCenter(i) = bin_start + (i + 0.5) * bin_width
 * With integer-valued centers the output string is unambiguous (no decimal).
 */

#include "PlotDataExport/HistogramCSVExport.hpp"

#include "CorePlotting/DataTypes/HistogramData.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

namespace {

/**
 * @brief Build a HistogramData with uniform bins starting at bin_start.
 * @param bin_start Left edge of the first bin
 * @param bin_width Width of each bin
 * @param counts    Per-bin values (count of elements per bin)
 */
CorePlotting::HistogramData makeHistogram(double bin_start,
                                          double bin_width,
                                          std::vector<double> counts) {
    CorePlotting::HistogramData h;
    h.bin_start = bin_start;
    h.bin_width = bin_width;
    h.counts = std::move(counts);
    return h;
}

}// namespace

// =============================================================================
// Empty-input tests
// =============================================================================

TEST_CASE("exportHistogramToCSV - empty series vector produces headers only",
          "[PlotDataExport][HistogramCSVExport]") {
    std::string const csv = PlotDataExport::exportHistogramToCSV({});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: psth"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("bin_center,event_key,value"));

    // Only 2 lines: comment + CSV header — no data rows
    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    REQUIRE(newlines == 2);
}

TEST_CASE("exportHistogramToCSV - empty HistogramData produces headers only",
          "[PlotDataExport][HistogramCSVExport]") {
    CorePlotting::HistogramData const empty;// counts is empty
    PlotDataExport::HistogramSeriesInput const input{"spikes", &empty};

    std::string const csv = PlotDataExport::exportHistogramToCSV({input});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("bin_center,event_key,value"));

    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    REQUIRE(newlines == 2);
}

// =============================================================================
// Single series correctness
// =============================================================================

TEST_CASE("exportHistogramToCSV - single series, single bin, known values",
          "[PlotDataExport][HistogramCSVExport]") {
    // bin_start=0, bin_width=10: binCenter(0) = 5.0 → "5"; count = 3.0 → "3"
    auto const hist = makeHistogram(0.0, 10.0, {3.0});
    PlotDataExport::HistogramSeriesInput const input{"spikes", &hist};

    std::string const csv = PlotDataExport::exportHistogramToCSV({input});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("bin_center,event_key,value\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5,spikes,3\n"));
}

TEST_CASE("exportHistogramToCSV - single series, multiple bins, correct bin_centers",
          "[PlotDataExport][HistogramCSVExport]") {
    // bin_start=0, bin_width=10, 3 bins
    // binCenter(0)=5, binCenter(1)=15, binCenter(2)=25
    auto const hist = makeHistogram(0.0, 10.0, {3.0, 7.0, 2.0});
    PlotDataExport::HistogramSeriesInput const input{"spikes", &hist};

    std::string const csv = PlotDataExport::exportHistogramToCSV({input});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5,spikes,3\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("15,spikes,7\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("25,spikes,2\n"));
}

TEST_CASE("exportHistogramToCSV - negative bin_start produces negative bin centers",
          "[PlotDataExport][HistogramCSVExport]") {
    // bin_start=-20, bin_width=10: binCenter(0)=-15, binCenter(1)=-5
    auto const hist = makeHistogram(-20.0, 10.0, {4.0, 6.0});
    PlotDataExport::HistogramSeriesInput const input{"spikes", &hist};

    std::string const csv = PlotDataExport::exportHistogramToCSV({input});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("-15,spikes,4\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("-5,spikes,6\n"));
}

// =============================================================================
// Zero-count bins must NOT be omitted
// =============================================================================

TEST_CASE("exportHistogramToCSV - zero-count bins appear in output",
          "[PlotDataExport][HistogramCSVExport]") {
    // Middle bin has zero count — it must still produce a row
    auto const hist = makeHistogram(0.0, 10.0, {3.0, 0.0, 7.0});
    PlotDataExport::HistogramSeriesInput const input{"spikes", &hist};

    std::string const csv = PlotDataExport::exportHistogramToCSV({input});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5,spikes,3\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("15,spikes,0\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("25,spikes,7\n"));

    // Verify all 3 data rows are present (header + comment + 3 data = 5 lines)
    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    REQUIRE(newlines == 5);
}

// =============================================================================
// Multiple series
// =============================================================================

TEST_CASE("exportHistogramToCSV - multiple series, correct event_key per row",
          "[PlotDataExport][HistogramCSVExport]") {
    auto const spikes_hist = makeHistogram(0.0, 10.0, {3.0, 7.0});
    auto const licks_hist = makeHistogram(0.0, 10.0, {1.0, 2.0});

    PlotDataExport::HistogramSeriesInput const in_spikes{"spikes", &spikes_hist};
    PlotDataExport::HistogramSeriesInput const in_licks{"licks", &licks_hist};

    std::string const csv =
            PlotDataExport::exportHistogramToCSV({in_spikes, in_licks});

    // spikes rows
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5,spikes,3\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("15,spikes,7\n"));

    // licks rows
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5,licks,1\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("15,licks,2\n"));
}

TEST_CASE("exportHistogramToCSV - series with different bin counts",
          "[PlotDataExport][HistogramCSVExport]") {
    // Two series may have different numbers of bins when produced by different
    // estimation methods. Each should produce its own rows independently.
    auto const short_hist = makeHistogram(0.0, 10.0, {5.0});    // 1 bin
    auto const long_hist = makeHistogram(0.0, 10.0, {5.0, 8.0});// 2 bins

    PlotDataExport::HistogramSeriesInput const in_a{"unit_a", &short_hist};
    PlotDataExport::HistogramSeriesInput const in_b{"unit_b", &long_hist};

    std::string const csv = PlotDataExport::exportHistogramToCSV({in_a, in_b});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5,unit_a,5\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5,unit_b,5\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("15,unit_b,8\n"));
}

// =============================================================================
// Custom delimiter
// =============================================================================

TEST_CASE("exportHistogramToCSV - custom tab delimiter",
          "[PlotDataExport][HistogramCSVExport]") {
    auto const hist = makeHistogram(0.0, 10.0, {3.0});
    PlotDataExport::HistogramSeriesInput const input{"spikes", &hist};

    std::string const csv =
            PlotDataExport::exportHistogramToCSV({input}, {}, "\t");

    REQUIRE_THAT(csv,
               Catch::Matchers::ContainsSubstring(
                       "bin_center\tevent_key\tvalue"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("5\tspikes\t3\n"));
}

// =============================================================================
// Metadata comment header
// =============================================================================

TEST_CASE("exportHistogramToCSV - metadata comment header written in full",
          "[PlotDataExport][HistogramCSVExport]") {
    PlotDataExport::HistogramExportMetadata meta;
    meta.alignment_key = "stimulus";
    meta.window_size = 1000.0;
    meta.scaling_mode = "FiringRateHz";
    meta.estimation_method = "Binning";

    std::string const csv = PlotDataExport::exportHistogramToCSV({}, meta);

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: psth"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# alignment_key: stimulus"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# window_size: 1000"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# scaling_mode: FiringRateHz"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# estimation_method: Binning"));
}

TEST_CASE("exportHistogramToCSV - optional metadata suppressed when empty",
          "[PlotDataExport][HistogramCSVExport]") {
    // Default-constructed metadata: all strings empty, window_size == 0
    std::string const csv = PlotDataExport::exportHistogramToCSV({});

    REQUIRE(csv.find("# alignment_key:") == std::string::npos);
    REQUIRE(csv.find("# window_size:") == std::string::npos);
    REQUIRE(csv.find("# scaling_mode:") == std::string::npos);
    REQUIRE(csv.find("# estimation_method:") == std::string::npos);
}
