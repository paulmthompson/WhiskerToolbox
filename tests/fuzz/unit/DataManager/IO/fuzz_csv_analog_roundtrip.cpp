/**
 * @file fuzz_csv_analog_roundtrip.cpp
 * @brief Fuzz tests for CSV AnalogTimeSeries round-trip (save → load → compare)
 *
 * Verifies that saving AnalogTimeSeries to CSV and loading it back produces
 * identical data within floating-point formatting precision.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"

#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>

namespace {

/**
 * @brief Round-trip fuzz test: generate AnalogTimeSeries, save as CSV, load back, compare.
 *
 * @param time_indices  Time frame indices (clamped to non-negative, deduplicated by map).
 * @param values        Sample values, one per time index.
 */
void FuzzCsvAnalogRoundTrip(
        std::vector<int> const & time_indices,
        std::vector<float> const & values) {

    auto const n = std::min(time_indices.size(), values.size());
    if (n == 0) {
        return;
    }

    // Skip if any value is non-finite (CSV formatting can't represent NaN/Inf reliably)
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(values[i])) {
            return;
        }
    }

    // Build vectors with non-negative, deduplicated time indices
    // Use a map to deduplicate (last value wins for duplicate times)
    std::map<int64_t, float> input_map;
    for (size_t i = 0; i < n; ++i) {
        auto const time = static_cast<int64_t>(std::abs(time_indices[i]));
        input_map[time] = values[i];
    }

    if (input_map.empty()) {
        return;
    }

    // Build AnalogTimeSeries from deduplicated data
    std::vector<float> data_values;
    std::vector<TimeFrameIndex> time_values;
    data_values.reserve(input_map.size());
    time_values.reserve(input_map.size());
    for (auto const & [t, v]: input_map) {
        time_values.emplace_back(t);
        data_values.push_back(v);
    }

    auto const analog_data = std::make_shared<AnalogTimeSeries>(data_values, time_values);

    // Save to a temp directory
    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_analog";
    std::filesystem::create_directories(temp_dir);

    CSVAnalogSaverOptions save_opts;
    save_opts.filename = "fuzz_analog.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = ",";
    save_opts.save_header = true;
    save_opts.header = "Time,Data";
    save_opts.precision = 6;

    bool const save_ok = save(analog_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    // Load back with matching settings
    CSVAnalogLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_analog.csv").string();
    load_opts.delimiter = ",";
    load_opts.has_header = true;
    load_opts.single_column_format = false;
    load_opts.time_column = 0;
    load_opts.data_column = 1;

    auto const loaded_data = load(load_opts);
    ASSERT_NE(loaded_data, nullptr);

    // Compare: same number of samples
    ASSERT_EQ(loaded_data->getNumSamples(), analog_data->getNumSamples());

    // Compare each sample's time and value within CSV formatting precision
    auto orig_samples = analog_data->getAllSamples();
    auto loaded_samples = loaded_data->getAllSamples();
    auto orig_it = orig_samples.begin();
    auto loaded_it = loaded_samples.begin();

    float constexpr tolerance = 1e-4F;
    while (orig_it != orig_samples.end() && loaded_it != loaded_samples.end()) {
        auto const & orig = *orig_it;
        auto const & loaded = *loaded_it;

        EXPECT_EQ(loaded.time_frame_index.getValue(), orig.time_frame_index.getValue())
                << "Time mismatch";
        EXPECT_NEAR(loaded.value(), orig.value(),
                    std::max(tolerance, std::abs(orig.value()) * 1e-5F))
                << "Value mismatch at time " << orig.time_frame_index.getValue();

        ++orig_it;
        ++loaded_it;
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvAnalogRoundTripFuzz, FuzzCsvAnalogRoundTrip)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100000)).WithMinSize(1).WithMaxSize(500),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(500));

/**
 * @brief Fuzz test with varying delimiter settings.
 */
void FuzzCsvAnalogRoundTripDelimiter(
        std::vector<int> const & time_indices,
        std::vector<float> const & values,
        std::string const & delimiter) {

    auto const n = std::min(time_indices.size(), values.size());
    if (n == 0 || delimiter.empty()) {
        return;
    }

    // Skip problematic delimiters
    for (char const c: delimiter) {
        if (c == '\n' || c == '\r' || std::isdigit(static_cast<unsigned char>(c)) ||
            c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
            return;
        }
    }

    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(values[i])) {
            return;
        }
    }

    std::map<int64_t, float> input_map;
    for (size_t i = 0; i < n; ++i) {
        auto const time = static_cast<int64_t>(std::abs(time_indices[i]));
        input_map[time] = values[i];
    }

    if (input_map.empty()) {
        return;
    }

    std::vector<float> data_values;
    std::vector<TimeFrameIndex> time_values;
    data_values.reserve(input_map.size());
    time_values.reserve(input_map.size());
    for (auto const & [t, v]: input_map) {
        time_values.emplace_back(t);
        data_values.push_back(v);
    }

    auto const analog_data = std::make_shared<AnalogTimeSeries>(data_values, time_values);

    auto const temp_dir = std::filesystem::temp_directory_path() / "whisker_fuzz_csv_analog_delim";
    std::filesystem::create_directories(temp_dir);

    CSVAnalogSaverOptions save_opts;
    save_opts.filename = "fuzz_analog_delim.csv";
    save_opts.parent_dir = temp_dir.string();
    save_opts.delimiter = delimiter;
    save_opts.save_header = false;
    save_opts.precision = 6;

    bool const save_ok = save(analog_data.get(), save_opts);
    if (!save_ok) {
        std::filesystem::remove_all(temp_dir);
        return;
    }

    CSVAnalogLoaderOptions load_opts;
    load_opts.filepath = (temp_dir / "fuzz_analog_delim.csv").string();
    load_opts.delimiter = delimiter.substr(0, 1);
    load_opts.has_header = false;
    load_opts.single_column_format = false;
    load_opts.time_column = 0;
    load_opts.data_column = 1;

    auto const loaded_data = load(load_opts);
    ASSERT_NE(loaded_data, nullptr);

    if (delimiter.size() == 1) {
        ASSERT_EQ(loaded_data->getNumSamples(), analog_data->getNumSamples());

        auto orig_samples = analog_data->getAllSamples();
        auto loaded_samples = loaded_data->getAllSamples();
        auto orig_it = orig_samples.begin();
        auto loaded_it = loaded_samples.begin();

        float constexpr tolerance = 1e-4F;
        while (orig_it != orig_samples.end() && loaded_it != loaded_samples.end()) {
            auto const & orig = *orig_it;
            auto const & loaded = *loaded_it;

            EXPECT_EQ(loaded.time_frame_index.getValue(), orig.time_frame_index.getValue());
            EXPECT_NEAR(loaded.value(), orig.value(),
                        std::max(tolerance, std::abs(orig.value()) * 1e-5F));

            ++orig_it;
            ++loaded_it;
        }
    }

    std::filesystem::remove_all(temp_dir);
}

FUZZ_TEST(CsvAnalogRoundTripFuzz, FuzzCsvAnalogRoundTripDelimiter)
        .WithDomains(
                fuzztest::VectorOf(fuzztest::InRange(0, 100000)).WithMinSize(1).WithMaxSize(200),
                fuzztest::VectorOf(fuzztest::InRange(-1e4F, 1e4F)).WithMinSize(1).WithMaxSize(200),
                fuzztest::OneOf(fuzztest::Just(std::string(",")),
                                fuzztest::Just(std::string(" ")),
                                fuzztest::Just(std::string("\t")),
                                fuzztest::Just(std::string(";"))));

}// namespace
