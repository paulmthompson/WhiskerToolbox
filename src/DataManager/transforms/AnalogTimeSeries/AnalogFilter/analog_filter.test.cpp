#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "transforms/ParameterFactory.hpp"
#include "utils/filter/FilterFactory.hpp"
#include "utils/filter/IFilter.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

TEST_CASE("Data Transform: Filter Analog Time Series", "[transforms][analog_filter]") {
    // Create test data: sine wave at 10 Hz sampled at 1000 Hz
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    const size_t num_samples = 2000;
    const double sampling_rate = 1000.0;
    const double signal_freq = 10.0;
    const double signal_freq_2 = 50.0;

    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        data.push_back(static_cast<float>(std::sin(2.0 * M_PI * signal_freq * t) + std::sin(2.0 * M_PI * signal_freq_2 * t)));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }

    auto series = std::make_shared<AnalogTimeSeries>(data, times);

    SECTION("Low-pass filter") {
        AnalogFilterParams params;
        params.filter_type = AnalogFilterParams::FilterType::Lowpass;
        params.cutoff_frequency = 20.0;
        params.order = 4;
        params.sampling_rate = sampling_rate;

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Check that high frequency content is attenuated
        float max_amplitude = 0.0f;
        for (size_t i = 500; i < filtered->getNumSamples(); ++i) {
            max_amplitude = std::max(max_amplitude,
                std::abs(filtered->getDataAtDataArrayIndex(DataArrayIndex(i))));
        }
        REQUIRE(max_amplitude < 1.1f); // Should be close to 1.0
        REQUIRE(max_amplitude > 0.9f);
    }

    SECTION("High-pass filter") {
        AnalogFilterParams params;
        params.filter_type = AnalogFilterParams::FilterType::Highpass;
        params.cutoff_frequency = 20.0;
        params.order = 4;
        params.sampling_rate = sampling_rate;

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Check that low frequency content is attenuated
        float max_amplitude = 0.0f;
        for (size_t i = 500; i < filtered->getNumSamples(); ++i) {
            max_amplitude = std::max(max_amplitude,
                std::abs(filtered->getDataAtDataArrayIndex(DataArrayIndex(i))));
        }
        REQUIRE(max_amplitude < 1.1f); // Should be close to 1.0
        REQUIRE(max_amplitude > 0.9f);
    }

    SECTION("Band-pass filter") {
        AnalogFilterParams params;
        params.filter_type = AnalogFilterParams::FilterType::Bandpass;
        params.cutoff_frequency = 5.0;
        params.cutoff_frequency2 = 15.0;
        params.order = 4;
        params.sampling_rate = sampling_rate;

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        float max_amplitude = 0.0f;
        for (size_t i = 500; i < filtered->getNumSamples(); ++i) {
            max_amplitude = std::max(max_amplitude,
                std::abs(filtered->getDataAtDataArrayIndex(DataArrayIndex(i))));
        }
        REQUIRE(max_amplitude < 1.1f);
        REQUIRE(max_amplitude > 0.9f);
    }

    SECTION("Band-stop filter") {
        AnalogFilterParams params;
        params.filter_type = AnalogFilterParams::FilterType::Bandstop;
        params.cutoff_frequency = 5.0;
        params.cutoff_frequency2 = 15.0;
        params.order = 4;
        params.sampling_rate = sampling_rate;

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        float max_amplitude = 0.0f;
        for (size_t i = 500; i < filtered->getNumSamples(); ++i) {
            max_amplitude = std::max(max_amplitude,
                std::abs(filtered->getDataAtDataArrayIndex(DataArrayIndex(i))));
        }
        REQUIRE(max_amplitude < 1.1f);
        REQUIRE(max_amplitude > 0.9f);
    }
}

// TEST_CASE("Data Transform: Filter Analog Time Series from JSON", "[transforms][analog_filter][json]") {
//     ParameterFactory::getInstance().initializeDefaultSetters();
//     DataManager dm;
//     std::vector<int> times;
//     for (int i = 0; i < 6; ++i) {
//         times.push_back(i);
//     }
//     auto time_frame = std::make_shared<TimeFrame>(times);
//     dm.setTime(TimeKey("single_column"), time_frame);

//     char const * json_config = R"([
//       {
//         "filepath": "data/Analog/single_column.csv",
//         "data_type": "analog",
//         "name": "my_analog_series",
//         "format": "csv"
//       },
//       {
//         "transformations": {
//             "steps": [
//                 {
//                     "step_id": "filter_analog",
//                     "transform_name": "Analog Filter",
//                     "input_key": "my_analog_series_0",
//                     "output_key": "filtered_series",
//                     "parameters": {
//                         "filter_type": "lowpass",
//                         "cutoff_frequency": 100,
//                         "order": 4,
//                         "sampling_rate": 1000.0
//                     }
//                 }
//             ]
//         }
//       }
//     ])";

//     load_data_from_json_config(&dm, nlohmann::json::parse(json_config), "bin");

//     auto filtered_series_optional = dm.getDataVariant("filtered_series");
//     REQUIRE(filtered_series_optional.has_value());
//     auto& filtered_series_variant = filtered_series_optional.value();
//     REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(filtered_series_variant));

//     auto filtered_series = std::get<std::shared_ptr<AnalogTimeSeries>>(filtered_series_variant);
//     REQUIRE(filtered_series);

//     auto original_series_optional = dm.getDataVariant("my_analog_series_0");
//     REQUIRE(original_series_optional.has_value());
//     auto& original_series_variant = original_series_optional.value();
//     REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(original_series_variant));
//     auto original_series = std::get<std::shared_ptr<AnalogTimeSeries>>(original_series_variant);
//     REQUIRE(original_series);

//     REQUIRE(filtered_series->getNumSamples() == original_series->getNumSamples());
// }