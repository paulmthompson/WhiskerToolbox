/**
 * @file fuzz_csv_event_roundtrip.cpp
 * @brief Fuzz tests for CSV DigitalEventSeries round-trip (save -> load -> compare)
 *
 * Verifies that saving DigitalEventSeries to CSV and loading it back produces
 * identical data (same event count, same event times).
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "IO/formats/CSV/digitaltimeseries/Digital_Event_Series_CSV.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

namespace {

/**
 * @brief Round-trip fuzz test: generate DigitalEventSeries, save as CSV, load back, compare.
 *
 * @param event_times  Fuzzed event frame indices (clamped to non-negative, deduplicated).
 */
void FuzzCsvEventRoundTrip(std::vector<int> const & event_times) {
    if (event_times.empty()) {
        return;
    }

    // Deduplicate and make non-negative
    std::set<int64_t> unique_times;
    for (auto const t: event_times) {
        unique_times.insert(static_cast<int64_t>(std::abs(t)));
    }

    if (unique_times.empty()) {
        return;
    }

    // Build DigitalEventSeries from deduplicated, sorted times
    std::vector<TimeFrameIndex> time_indices;
    time_indices.reserve(unique_times.size());
    for (auto const t: unique_times) {
        time_indices.emplace_back(t);
    }

    auto const event_data = std::make_shared<DigitalEventSeries>(time_indices);

    // Save to a temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_event";
    std::filesystem::create_directories(temp_dir);

    CSVEventSaverOptions save_opts;
    save_opts.filename = "fuzz_events.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.save_header = true;
    save_opts.header = "Event";
    save_opts.precision = 3;

    bool const save_ok = save(event_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back with matching settings
    CSVEventLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_events.csv").string();
    load_opts.delimiter = ",";
    load_opts.has_header = true;
    load_opts.event_column = 0;
    load_opts.identifier_column = -1;
    load_opts.base_name = "fuzz_events";

    auto const loaded_series = load(load_opts);
    ASSERT_EQ(loaded_series.size(), 1U);
    ASSERT_NE(loaded_series[0], nullptr);

    auto const & loaded = *loaded_series[0];

    // Compare: same event count
    ASSERT_EQ(loaded.size(), event_data->size());

    // Compare: same event times (integer-based, should match exactly)
    auto orig_view = event_data->view();
    auto loaded_view = loaded.view();

    for (size_t i = 0; i < event_data->size(); ++i) {
        EXPECT_EQ(loaded_view[i].time().getValue(), orig_view[i].time().getValue())
                << "Event time mismatch at index " << i;
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvEventRoundTripFuzz, FuzzCsvEventRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100000)).WithMinSize(1).WithMaxSize(1000));

/**
 * @brief Fuzz test with varying delimiter settings.
 */
void FuzzCsvEventRoundTripDelimiter(
        std::vector<int> const & event_times,
        std::string const & delimiter) {

    if (event_times.empty() || delimiter.empty()) {
        return;
    }

    // Skip problematic delimiters
    for (char const c: delimiter) {
        if (c == '\n' || c == '\r' || std::isdigit(static_cast<unsigned char>(c)) ||
            c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
            return;
        }
    }

    // Deduplicate and make non-negative
    std::set<int64_t> unique_times;
    for (auto const t: event_times) {
        unique_times.insert(static_cast<int64_t>(std::abs(t)));
    }

    if (unique_times.empty()) {
        return;
    }

    std::vector<TimeFrameIndex> time_indices;
    time_indices.reserve(unique_times.size());
    for (auto const t: unique_times) {
        time_indices.emplace_back(t);
    }

    auto const event_data = std::make_shared<DigitalEventSeries>(time_indices);

    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_event_delim";
    std::filesystem::create_directories(temp_dir);

    CSVEventSaverOptions save_opts;
    save_opts.filename = "fuzz_events_delim.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = delimiter;
    save_opts.save_header = false;
    save_opts.precision = 3;

    bool const save_ok = save(event_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    CSVEventLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_events_delim.csv").string();
    load_opts.delimiter = delimiter.substr(0, 1);
    load_opts.has_header = false;
    load_opts.event_column = 0;
    load_opts.identifier_column = -1;

    auto const loaded_series = load(load_opts);
    ASSERT_EQ(loaded_series.size(), 1U);
    ASSERT_NE(loaded_series[0], nullptr);

    auto const & loaded = *loaded_series[0];

    if (delimiter.size() == 1) {
        ASSERT_EQ(loaded.size(), event_data->size());

        auto orig_view = event_data->view();
        auto loaded_view = loaded.view();

        for (size_t i = 0; i < event_data->size(); ++i) {
            EXPECT_EQ(loaded_view[i].time().getValue(), orig_view[i].time().getValue())
                    << "Event time mismatch at index " << i;
        }
    }

    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvEventRoundTripFuzz, FuzzCsvEventRoundTripDelimiter)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100000)).WithMinSize(1).WithMaxSize(500),
                fuzztest::OneOf(fuzztest::Just(std::string(",")),
                                fuzztest::Just(std::string(" ")),
                                fuzztest::Just(std::string("\t")),
                                fuzztest::Just(std::string(";"))));

}// namespace
