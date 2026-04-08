/**
 * @file test_HeatmapCSVExport.cpp
 * @brief Unit tests for HeatmapCSVExport (aggregate and per-trial heatmap CSV export)
 *
 * Tests cover both export modes:
 *   - exportHeatmapToCSV()       — wide matrix, one row per unit
 *   - exportHeatmapPerTrialToCSV() — long-form tidy, one row per (unit, trial, bin)
 *
 * For aggregate export, bin centers are derived from the first row's bin_start
 * and bin_width:  center_i = bin_start + (i + 0.5) * bin_width
 *
 * For per-trial export, bin centers are stored explicitly in PerTrialHeatmapInput::times.
 */

#include "PlotDataExport/HeatmapCSVExport.hpp"

#include "CorePlotting/Mappers/HeatmapMapper.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

namespace {

CorePlotting::HeatmapRowData makeRow(double bin_start,
                                     double bin_width,
                                     std::vector<double> values) {
    CorePlotting::HeatmapRowData row;
    row.bin_start = bin_start;
    row.bin_width = bin_width;
    row.values = std::move(values);
    return row;
}

PlotDataExport::PerTrialHeatmapInput makePerTrialInput(
        std::string unit_key,
        std::vector<double> times,
        std::vector<std::vector<double>> per_trial_values) {
    PlotDataExport::PerTrialHeatmapInput input;
    input.unit_key = std::move(unit_key);
    input.times = std::move(times);
    input.per_trial_values = std::move(per_trial_values);
    return input;
}

}// namespace

// =============================================================================
// exportHeatmapToCSV — aggregate / wide-matrix mode
// =============================================================================

TEST_CASE("exportHeatmapToCSV - empty rows produces comment + unit_key header only",
          "[PlotDataExport][HeatmapCSVExport]") {
    std::string const csv = PlotDataExport::exportHeatmapToCSV({}, {});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: heatmap_aggregate"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_key\n"));
}

TEST_CASE("exportHeatmapToCSV - single unit, single bin, known values",
          "[PlotDataExport][HeatmapCSVExport]") {
    // bin_start=0, bin_width=10: center=5  →  header "unit_key,5"
    // value=2.5 → data row "unit_a,2.5"
    auto const row = makeRow(0.0, 10.0, {2.5});

    std::string const csv =
            PlotDataExport::exportHeatmapToCSV({"unit_a"}, {row});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_key,5\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,2.5\n"));
}

TEST_CASE("exportHeatmapToCSV - single unit, multiple bins",
          "[PlotDataExport][HeatmapCSVExport]") {
    // bin_start=0, bin_width=10, 3 bins: centers 5, 15, 25
    auto const row = makeRow(0.0, 10.0, {1.0, 2.0, 3.0});

    std::string const csv =
            PlotDataExport::exportHeatmapToCSV({"unit_a"}, {row});

    // Header contains all bin centers
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_key,5,15,25\n"));
    // Data row contains all values
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,1,2,3\n"));
}

TEST_CASE("exportHeatmapToCSV - multiple units, each gets its own row",
          "[PlotDataExport][HeatmapCSVExport]") {
    auto const row_a = makeRow(0.0, 10.0, {1.0, 2.0});
    auto const row_b = makeRow(0.0, 10.0, {5.0, 6.0});

    std::vector<std::string> const keys{"unit_a", "unit_b"};
    std::vector<CorePlotting::HeatmapRowData> const rows{row_a, row_b};

    std::string const csv = PlotDataExport::exportHeatmapToCSV(keys, rows);

    // Both units present, with their own values
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,1,2\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_b,5,6\n"));
    // Shared header row (columns derived from first row)
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_key,5,15\n"));
}

TEST_CASE("exportHeatmapToCSV - custom delimiter",
          "[PlotDataExport][HeatmapCSVExport]") {
    auto const row = makeRow(0.0, 10.0, {3.0, 4.0});

    std::string const csv =
            PlotDataExport::exportHeatmapToCSV({"unit_a"}, {row}, {}, "\t");

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_key\t5\t15\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a\t3\t4\n"));
}

TEST_CASE("exportHeatmapToCSV - metadata comment header written",
          "[PlotDataExport][HeatmapCSVExport]") {
    PlotDataExport::HeatmapExportMetadata meta;
    meta.alignment_key = "stim";
    meta.window_size = 500.0;
    meta.scaling_mode = "FiringRateHz";
    meta.estimation_method = "Binning";

    std::string const csv = PlotDataExport::exportHeatmapToCSV({}, {}, meta);

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: heatmap_aggregate"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# alignment_key: stim"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# window_size: 500"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# scaling_mode: FiringRateHz"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# estimation_method: Binning"));
}

TEST_CASE("exportHeatmapToCSV - optional metadata suppressed when default",
          "[PlotDataExport][HeatmapCSVExport]") {
    std::string const csv = PlotDataExport::exportHeatmapToCSV({}, {});

    REQUIRE(csv.find("# alignment_key:") == std::string::npos);
    REQUIRE(csv.find("# window_size:") == std::string::npos);
    REQUIRE(csv.find("# scaling_mode:") == std::string::npos);
    REQUIRE(csv.find("# estimation_method:") == std::string::npos);
}

// =============================================================================
// exportHeatmapPerTrialToCSV — per-trial / long-form tidy mode
// =============================================================================

TEST_CASE("exportHeatmapPerTrialToCSV - empty data produces headers only",
          "[PlotDataExport][HeatmapCSVExport]") {
    std::string const csv = PlotDataExport::exportHeatmapPerTrialToCSV({});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: heatmap_per_trial"));
    REQUIRE_THAT(csv,
               Catch::Matchers::ContainsSubstring(
                       "unit_key,trial_index,bin_center,value\n"));

    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    REQUIRE(newlines == 2);
}

TEST_CASE("exportHeatmapPerTrialToCSV - single unit, single trial, single bin",
          "[PlotDataExport][HeatmapCSVExport]") {
    // times={5.0}, per_trial_values={{3.0}}
    auto const input = makePerTrialInput("unit_a", {5.0}, {{3.0}});

    std::string const csv = PlotDataExport::exportHeatmapPerTrialToCSV({input});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring(
                            "unit_key,trial_index,bin_center,value\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,0,5,3\n"));
}

TEST_CASE("exportHeatmapPerTrialToCSV - single unit, multiple trials",
          "[PlotDataExport][HeatmapCSVExport]") {
    // 2 trials × 2 bins each
    // times={5,15}, trial 0 values={1,2}, trial 1 values={3,4}
    auto const input = makePerTrialInput("unit_a", {5.0, 15.0}, {{1.0, 2.0}, {3.0, 4.0}});

    std::string const csv = PlotDataExport::exportHeatmapPerTrialToCSV({input});

    // Trial 0
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,0,5,1\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,0,15,2\n"));
    // Trial 1
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,1,5,3\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,1,15,4\n"));
}

TEST_CASE("exportHeatmapPerTrialToCSV - multiple units",
          "[PlotDataExport][HeatmapCSVExport]") {
    auto const unit_a = makePerTrialInput("unit_a", {5.0}, {{1.0}, {2.0}});
    auto const unit_b = makePerTrialInput("unit_b", {5.0}, {{7.0}, {8.0}});

    std::string const csv =
            PlotDataExport::exportHeatmapPerTrialToCSV({unit_a, unit_b});

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,0,5,1\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a,1,5,2\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_b,0,5,7\n"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_b,1,5,8\n"));
}

TEST_CASE("exportHeatmapPerTrialToCSV - custom delimiter",
          "[PlotDataExport][HeatmapCSVExport]") {
    auto const input = makePerTrialInput("unit_a", {5.0}, {{3.0}});

    std::string const csv =
            PlotDataExport::exportHeatmapPerTrialToCSV({input}, {}, "\t");

    REQUIRE_THAT(csv,
               Catch::Matchers::ContainsSubstring(
                       "unit_key\ttrial_index\tbin_center\tvalue"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("unit_a\t0\t5\t3\n"));
}

TEST_CASE("exportHeatmapPerTrialToCSV - metadata comment header written",
          "[PlotDataExport][HeatmapCSVExport]") {
    PlotDataExport::HeatmapExportMetadata meta;
    meta.alignment_key = "stim";
    meta.window_size = 500.0;

    std::string const csv =
            PlotDataExport::exportHeatmapPerTrialToCSV({}, meta);

    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# export_type: heatmap_per_trial"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# alignment_key: stim"));
    REQUIRE_THAT(csv, Catch::Matchers::ContainsSubstring("# window_size: 500"));
}

TEST_CASE("exportHeatmapPerTrialToCSV - unit with no trials produces no data rows",
          "[PlotDataExport][HeatmapCSVExport]") {
    // per_trial_values is empty → zero trials
    auto const input = makePerTrialInput("unit_a", {5.0, 15.0}, {});

    std::string const csv = PlotDataExport::exportHeatmapPerTrialToCSV({input});

    REQUIRE(csv.find("unit_a,") == std::string::npos);

    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    REQUIRE(newlines == 2);// comment + header only
}

TEST_CASE("exportHeatmapPerTrialToCSV - row count matches units x trials x bins",
          "[PlotDataExport][HeatmapCSVExport]") {
    // 2 units × 3 trials × 2 bins = 12 data rows
    auto const unit_a = makePerTrialInput(
            "unit_a", {5.0, 15.0},
            {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}});
    auto const unit_b = makePerTrialInput(
            "unit_b", {5.0, 15.0},
            {{7.0, 8.0}, {9.0, 10.0}, {11.0, 12.0}});

    std::string const csv =
            PlotDataExport::exportHeatmapPerTrialToCSV({unit_a, unit_b});

    // Count newlines: 1 comment + 1 header + 12 data = 14
    int newlines = 0;
    for (char const c: csv) {
        if (c == '\n') {
            ++newlines;
        }
    }
    REQUIRE(newlines == 14);
}
