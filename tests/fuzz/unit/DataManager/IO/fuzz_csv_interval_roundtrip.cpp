/**
 * @file fuzz_csv_interval_roundtrip.cpp
 * @brief Fuzz tests for CSV DigitalIntervalSeries round-trip (save -> load -> compare)
 *
 * Verifies that saving DigitalIntervalSeries to CSV and loading it back produces
 * identical data (same interval count, same start/end times).
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "IO/formats/CSV/digitaltimeseries/Digital_Interval_Series_CSV.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

namespace {

/**
 * @brief Round-trip fuzz test: generate DigitalIntervalSeries, save as CSV, load back, compare.
 *
 * @param start_times  Fuzzed interval start times (clamped to non-negative, sorted, non-overlapping).
 * @param durations    Fuzzed interval durations (positive, paired with starts).
 */
void FuzzCsvIntervalRoundTrip(
        std::vector<int> const & start_times,
        std::vector<int> const & durations) {

    if (start_times.empty() || durations.empty()) {
        return;
    }

    // Build non-overlapping intervals from fuzzed inputs
    std::vector<Interval> intervals;
    auto const count = std::min(start_times.size(), durations.size());

    // Sort starts and ensure non-overlapping by using cumulative offsets
    std::vector<int> sorted_starts(start_times.begin(),
                                   start_times.begin() + static_cast<std::ptrdiff_t>(count));
    std::sort(sorted_starts.begin(), sorted_starts.end());

    int64_t next_valid_start = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t const start = std::max(
                static_cast<int64_t>(std::abs(sorted_starts[i])),
                next_valid_start);
        int64_t const dur = std::max(static_cast<int64_t>(std::abs(durations[i])), int64_t{1});
        int64_t const end = start + dur;

        intervals.emplace_back(Interval{start, end});
        next_valid_start = end + 2;// gap of at least 2 to avoid adjacency issues
    }

    if (intervals.empty()) {
        return;
    }

    auto const interval_data = std::make_shared<DigitalIntervalSeries>(intervals);

    // Save to a temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_interval";
    std::filesystem::create_directories(temp_dir);

    CSVIntervalSaverOptions save_opts;
    save_opts.filename = "fuzz_intervals.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.save_header = true;
    save_opts.header = "Start,End";

    bool const save_ok = save(interval_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back with matching settings
    CSVIntervalLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_intervals.csv").string();
    load_opts.delimiter = ",";
    load_opts.has_header = true;
    load_opts.start_column = 0;
    load_opts.end_column = 1;

    auto const loaded_intervals = load(load_opts);
    ASSERT_EQ(loaded_intervals.size(), intervals.size());

    auto const loaded_data = std::make_shared<DigitalIntervalSeries>(loaded_intervals);

    // Compare: same interval count
    ASSERT_EQ(loaded_data->size(), interval_data->size());

    // Compare: same start/end times (integer-based, should match exactly)
    auto orig_view = interval_data->view();
    auto loaded_view = loaded_data->view();

    for (size_t i = 0; i < interval_data->size(); ++i) {
        EXPECT_EQ(loaded_view[i].value().start, orig_view[i].value().start)
                << "Interval start mismatch at index " << i;
        EXPECT_EQ(loaded_view[i].value().end, orig_view[i].value().end)
                << "Interval end mismatch at index " << i;
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvIntervalRoundTripFuzz, FuzzCsvIntervalRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100000)).WithMinSize(1).WithMaxSize(500),
                fuzztest::VectorOf(fuzztest::InRange(1, 10000)).WithMinSize(1).WithMaxSize(500));

/**
 * @brief Fuzz test with varying delimiter settings.
 */
void FuzzCsvIntervalRoundTripDelimiter(
        std::vector<int> const & start_times,
        std::vector<int> const & durations,
        std::string const & delimiter) {

    if (start_times.empty() || durations.empty() || delimiter.empty()) {
        return;
    }

    // Skip problematic delimiters
    for (char const c: delimiter) {
        if (c == '\n' || c == '\r' || std::isdigit(static_cast<unsigned char>(c)) ||
            c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
            return;
        }
    }

    // Build non-overlapping intervals
    std::vector<Interval> intervals;
    auto const count = std::min(start_times.size(), durations.size());

    std::vector<int> sorted_starts(start_times.begin(),
                                   start_times.begin() + static_cast<std::ptrdiff_t>(count));
    std::sort(sorted_starts.begin(), sorted_starts.end());

    int64_t next_valid_start = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t const start = std::max(
                static_cast<int64_t>(std::abs(sorted_starts[i])),
                next_valid_start);
        int64_t const dur = std::max(static_cast<int64_t>(std::abs(durations[i])), int64_t{1});
        int64_t const end = start + dur;

        intervals.emplace_back(Interval{start, end});
        next_valid_start = end + 2;
    }

    if (intervals.empty()) {
        return;
    }

    auto const interval_data = std::make_shared<DigitalIntervalSeries>(intervals);

    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_interval_delim";
    std::filesystem::create_directories(temp_dir);

    CSVIntervalSaverOptions save_opts;
    save_opts.filename = "fuzz_intervals_delim.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = delimiter;
    save_opts.save_header = false;

    bool const save_ok = save(interval_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    CSVIntervalLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_intervals_delim.csv").string();
    load_opts.delimiter = delimiter.substr(0, 1);
    load_opts.has_header = false;
    load_opts.start_column = 0;
    load_opts.end_column = 1;

    auto const loaded_intervals = load(load_opts);

    if (delimiter.size() == 1) {
        ASSERT_EQ(loaded_intervals.size(), intervals.size());

        auto const loaded_data = std::make_shared<DigitalIntervalSeries>(loaded_intervals);
        auto orig_view = interval_data->view();
        auto loaded_view = loaded_data->view();

        for (size_t i = 0; i < interval_data->size(); ++i) {
            EXPECT_EQ(loaded_view[i].value().start, orig_view[i].value().start)
                    << "Interval start mismatch at index " << i;
            EXPECT_EQ(loaded_view[i].value().end, orig_view[i].value().end)
                    << "Interval end mismatch at index " << i;
        }
    }

    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvIntervalRoundTripFuzz, FuzzCsvIntervalRoundTripDelimiter)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100000)).WithMinSize(1).WithMaxSize(500),
                fuzztest::VectorOf(fuzztest::InRange(1, 10000)).WithMinSize(1).WithMaxSize(500),
                fuzztest::OneOf(fuzztest::Just(std::string(",")),
                                fuzztest::Just(std::string(" ")),
                                fuzztest::Just(std::string("\t")),
                                fuzztest::Just(std::string(";"))));

}// namespace
