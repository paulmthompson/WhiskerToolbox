#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "utils/filter/FilterFactory.hpp"
#include "utils/filter/IFilter.hpp"

#include "fixtures/AnalogFilterTestFixture.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <fstream>
#include <iostream>


TEST_CASE_METHOD(AnalogFilterTestFixture, "Data Transform: Filter Analog Time Series", "[transforms][analog_filter]") {
    auto series = m_test_analog_signals["sine_10hz_2000"];
    const size_t num_samples = 2000;
    const double sampling_rate = 1000.0;
    const double signal_freq = 10.0;

    SECTION("Low-pass filter") {
        // Use the new filter interface with Butterworth lowpass
        auto filter = FilterFactory::createButterworthLowpass<4>(3.0, sampling_rate, false);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Check that high frequency content is attenuated
        float max_amplitude = 0.0f;
        // Skip initial transient
        size_t i = 0;
        for (auto const& sample : filtered->getAllSamples()) {
            if (i >= 500) {
                max_amplitude = std::max(max_amplitude, std::abs(sample.value()));
            }
            ++i;
        }
        REQUIRE(max_amplitude < 0.15f); // More stringent requirement
    }

    SECTION("High-pass filter") {
        // Use the new filter interface with Butterworth highpass
        auto filter = FilterFactory::createButterworthHighpass<4>(20.0, sampling_rate, false);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Check that low frequency content is attenuated
        float max_amplitude = 0.0f;
        // Skip initial transient
        size_t i = 0;
        for (auto const& sample : filtered->getAllSamples()) {
            if (i >= 500) {
                max_amplitude = std::max(max_amplitude, std::abs(sample.value()));
            }
            ++i;
        }
        REQUIRE(max_amplitude < 0.15f); // More stringent requirement
    }

    SECTION("Band-pass filter around signal frequency") {
        // Use the new filter interface with Butterworth bandpass
        auto filter = FilterFactory::createButterworthBandpass<4>(9.0, 11.0, sampling_rate, false);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);

        // Signal should be preserved (allowing for some attenuation)
        float max_amplitude = 0.0f;
        // Skip longer transient for bandpass
        size_t i = 0;
        for (auto const& sample : filtered->getAllSamples()) {
            if (i >= 500) {
                max_amplitude = std::max(max_amplitude, std::abs(sample.value()));
            }
            ++i;
        }
        REQUIRE(max_amplitude > 0.7f); // Signal should be mostly preserved
    }

    SECTION("Notch filter at signal frequency") {
        // Create a notch filter at 10 Hz with very narrow bandwidth using RBJ
        auto filter = FilterFactory::createRBJBandstop(signal_freq, sampling_rate, 200.0, true); // Zero-phase for better attenuation
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);

        // Get filtered data
        auto filtered_data = filtered->getAnalogTimeSeries();

        // Skip first 1000 samples to avoid transient response
        float max_amplitude = 0.0f;
        for (size_t i = 1000; i < filtered_data.size(); ++i) {
            max_amplitude = std::max(max_amplitude, std::abs(filtered_data[i]));
        }

        // The notch filter should significantly attenuate the 10 Hz signal
        REQUIRE(max_amplitude < 0.2);
    }
}

TEST_CASE_METHOD(AnalogFilterTestFixture,"Data Transform: Filter Analog Time Series - Operation Class Tests", "[transforms][analog_filter][operation]") {
    AnalogFilterOperation operation;

    SECTION("Basic operation properties") {
        REQUIRE(operation.getName() == "Filter");
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<AnalogTimeSeries>));
    }

    SECTION("Default parameters") {
        auto params = operation.getDefaultParameters();
        REQUIRE(params);
        auto* filterParams = dynamic_cast<AnalogFilterParams*>(params.get());
        REQUIRE(filterParams);
    }

    SECTION("Can apply to correct type") {
        auto series = std::make_shared<AnalogTimeSeries>();
        DataTypeVariant variant(series);
        REQUIRE(operation.canApply(variant));
    }

    SECTION("Execute with progress callback") {
        auto series = m_test_analog_signals["constant_1000"];
        DataTypeVariant input(series);

        // Create parameters using new filter interface
        auto filter = FilterFactory::createButterworthLowpass<4>(10.0, 100.0, false);
        auto params = std::make_unique<AnalogFilterParams>(
            AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter))));

        bool progress_called = false;
        auto result = operation.execute(input, params.get(), 
            [&progress_called](int progress) {
                if (progress == 100) progress_called = true;
            });

        REQUIRE(progress_called);
        REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
        auto filtered = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == 1000);
    }
}

TEST_CASE_METHOD(AnalogFilterTestFixture, "Data Transform: Filter Analog Time Series - New Interface Features", "[transforms][analog_filter][new_interface]") {
    auto series = m_test_analog_signals["pattern_1000"];
    const size_t num_samples = 1000;
    const double sampling_rate = 1000.0;

    SECTION("Filter factory function approach") {
        // Create parameters using factory function
        auto params = AnalogFilterParams::withFactory([]() {
            return FilterFactory::createButterworthLowpass<2>(50.0, 1000.0, false);
        });

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }

    SECTION("Direct filter instance approach") {
        // Create filter instance directly
        auto filter = FilterFactory::createChebyshevILowpass<3>(100.0, 1000.0, 1.0, true);
        auto params = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter)));

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
        
        // Check that filter name is available
        REQUIRE_FALSE(params.getFilterName().empty());
        REQUIRE(params.getFilterName().find("Chebyshev I") != std::string::npos);
    }

    SECTION("Default parameters test") {
        // Test that default parameters work correctly
        auto params = AnalogFilterParams::createDefault(1000.0, 75.0);

        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }

    SECTION("Default constructor test") {
        // Test that default constructor creates a working filter
        auto params = AnalogFilterParams();  // Should use default constructor

        REQUIRE(params.isValid());
        REQUIRE_FALSE(params.getFilterName().empty());
        
        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }

    SECTION("Different filter types comparison") {
        // Compare different filter types on the same data
        auto butterworth = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, false);
        auto chebyshev1 = FilterFactory::createChebyshevILowpass<4>(50.0, sampling_rate, 1.0, false);
        auto chebyshev2 = FilterFactory::createChebyshevIILowpass<4>(50.0, sampling_rate, 20.0, false);
        auto rbj = FilterFactory::createRBJLowpass(50.0, sampling_rate, 0.707, false);

        auto params_butter = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(butterworth)));
        auto params_cheby1 = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(chebyshev1)));
        auto params_cheby2 = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(chebyshev2)));
        auto params_rbj = AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(rbj)));

        auto filtered_butter = filter_analog(series.get(), params_butter);
        auto filtered_cheby1 = filter_analog(series.get(), params_cheby1);
        auto filtered_cheby2 = filter_analog(series.get(), params_cheby2);
        auto filtered_rbj = filter_analog(series.get(), params_rbj);

        REQUIRE(filtered_butter);
        REQUIRE(filtered_cheby1);
        REQUIRE(filtered_cheby2);
        REQUIRE(filtered_rbj);

        // All should have same number of samples
        REQUIRE(filtered_butter->getNumSamples() == num_samples);
        REQUIRE(filtered_cheby1->getNumSamples() == num_samples);
        REQUIRE(filtered_cheby2->getNumSamples() == num_samples);
        REQUIRE(filtered_rbj->getNumSamples() == num_samples);
    }
}

TEST_CASE("Data Transform: Filter Specification - Validation", "[transforms][analog_filter][specification]") {
    SECTION("Valid Butterworth lowpass specification") {
        FilterSpecification spec;
        spec.family = FilterFamily::Butterworth;
        spec.response = FilterResponse::Lowpass;
        spec.order = 4;
        spec.cutoff_hz = 10.0;
        spec.sampling_rate_hz = 1000.0;
        spec.zero_phase = false;
        
        REQUIRE(spec.isValid());
        REQUIRE(spec.validate().empty());
    }
    
    SECTION("Invalid order") {
        FilterSpecification spec;
        spec.order = 10;  // Too high
        
        auto errors = spec.validate();
        REQUIRE_FALSE(errors.empty());
        REQUIRE(errors[0].find("order") != std::string::npos);
    }
    
    SECTION("Invalid sampling rate") {
        FilterSpecification spec;
        spec.sampling_rate_hz = -100.0;
        
        auto errors = spec.validate();
        REQUIRE_FALSE(errors.empty());
        REQUIRE(errors[0].find("Sampling rate") != std::string::npos);
    }
    
    SECTION("Cutoff above Nyquist") {
        FilterSpecification spec;
        spec.cutoff_hz = 600.0;
        spec.sampling_rate_hz = 1000.0;  // Nyquist = 500 Hz
        
        auto errors = spec.validate();
        REQUIRE_FALSE(errors.empty());
        REQUIRE(errors[0].find("Nyquist") != std::string::npos);
    }
    
    SECTION("Invalid bandpass frequencies") {
        FilterSpecification spec;
        spec.response = FilterResponse::Bandpass;
        spec.cutoff_low_hz = 50.0;
        spec.cutoff_high_hz = 30.0;  // Low > High
        spec.sampling_rate_hz = 1000.0;
        
        auto errors = spec.validate();
        REQUIRE_FALSE(errors.empty());
        REQUIRE(errors[0].find("Low cutoff") != std::string::npos);
    }
    
    SECTION("Missing ripple for Chebyshev") {
        FilterSpecification spec;
        spec.family = FilterFamily::ChebyshevI;
        spec.ripple_db = -1.0;  // Invalid
        
        auto errors = spec.validate();
        REQUIRE_FALSE(errors.empty());
        REQUIRE(errors[0].find("Ripple") != std::string::npos);
    }
    
    SECTION("RBJ only supports bandstop") {
        FilterSpecification spec;
        spec.family = FilterFamily::RBJ;
        spec.response = FilterResponse::Lowpass;  // Invalid for RBJ
        
        auto errors = spec.validate();
        REQUIRE_FALSE(errors.empty());
        REQUIRE(errors[0].find("RBJ") != std::string::npos);
    }
}

TEST_CASE("Data Transform: Filter Specification - JSON Serialization", "[transforms][analog_filter][specification][json]") {
    SECTION("Butterworth lowpass roundtrip") {
        FilterSpecification spec;
        spec.family = FilterFamily::Butterworth;
        spec.response = FilterResponse::Lowpass;
        spec.order = 4;
        spec.cutoff_hz = 10.0;
        spec.sampling_rate_hz = 1000.0;
        spec.zero_phase = true;
        
        auto json = spec.toJson();
        auto spec2 = FilterSpecification::fromJson(json);
        
        REQUIRE(spec2.family == spec.family);
        REQUIRE(spec2.response == spec.response);
        REQUIRE(spec2.order == spec.order);
        REQUIRE(spec2.cutoff_hz == spec.cutoff_hz);
        REQUIRE(spec2.sampling_rate_hz == spec.sampling_rate_hz);
        REQUIRE(spec2.zero_phase == spec.zero_phase);
    }
    
    SECTION("Chebyshev I bandpass roundtrip") {
        FilterSpecification spec;
        spec.family = FilterFamily::ChebyshevI;
        spec.response = FilterResponse::Bandpass;
        spec.order = 6;
        spec.cutoff_low_hz = 5.0;
        spec.cutoff_high_hz = 20.0;
        spec.sampling_rate_hz = 100.0;
        spec.ripple_db = 0.5;
        spec.zero_phase = false;
        
        auto json = spec.toJson();
        auto spec2 = FilterSpecification::fromJson(json);
        
        REQUIRE(spec2.family == spec.family);
        REQUIRE(spec2.response == spec.response);
        REQUIRE(spec2.order == spec.order);
        REQUIRE(spec2.cutoff_low_hz == spec.cutoff_low_hz);
        REQUIRE(spec2.cutoff_high_hz == spec.cutoff_high_hz);
        REQUIRE(spec2.sampling_rate_hz == spec.sampling_rate_hz);
        REQUIRE(spec2.ripple_db == spec.ripple_db);
        REQUIRE(spec2.zero_phase == spec.zero_phase);
    }
    
    SECTION("RBJ notch roundtrip") {
        FilterSpecification spec;
        spec.family = FilterFamily::RBJ;
        spec.response = FilterResponse::Bandstop;
        spec.cutoff_hz = 60.0;  // 60 Hz notch
        spec.sampling_rate_hz = 1000.0;
        spec.q_factor = 30.0;
        spec.zero_phase = true;
        
        auto json = spec.toJson();
        auto spec2 = FilterSpecification::fromJson(json);
        
        REQUIRE(spec2.family == spec.family);
        REQUIRE(spec2.response == spec.response);
        REQUIRE(spec2.cutoff_hz == spec.cutoff_hz);
        REQUIRE(spec2.sampling_rate_hz == spec.sampling_rate_hz);
        REQUIRE(spec2.q_factor == spec.q_factor);
        REQUIRE(spec2.zero_phase == spec.zero_phase);
    }
    
    SECTION("Invalid JSON missing required field") {
        nlohmann::json json = {
            {"filter_family", "butterworth"},
            {"order", 4}
            // Missing filter_response
        };
        
        REQUIRE_THROWS_AS(FilterSpecification::fromJson(json), std::invalid_argument);
    }
    
    SECTION("Invalid JSON unknown filter family") {
        nlohmann::json json = {
            {"filter_family", "unknown_filter"},
            {"filter_response", "lowpass"},
            {"order", 4},
            {"cutoff_hz", 10.0},
            {"sampling_rate_hz", 1000.0}
        };
        
        REQUIRE_THROWS_AS(FilterSpecification::fromJson(json), std::invalid_argument);
    }
}

TEST_CASE_METHOD(AnalogFilterTestFixture, "Data Transform: Filter Specification - Filter Creation", "[transforms][analog_filter][specification]") {
    auto series = m_test_analog_signals["sine_10hz_1000"];
    const size_t num_samples = 1000;
    
    SECTION("Create and apply Butterworth lowpass") {
        FilterSpecification spec;
        spec.family = FilterFamily::Butterworth;
        spec.response = FilterResponse::Lowpass;
        spec.order = 4;
        spec.cutoff_hz = 5.0;
        spec.sampling_rate_hz = 1000.0;
        spec.zero_phase = false;
        
        auto filter = spec.createFilter();
        REQUIRE(filter);
        REQUIRE(filter->getName().find("Butterworth") != std::string::npos);
        
        // Apply filter
        auto params = AnalogFilterParams::withSpecification(spec);
        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }
    
    SECTION("Create and apply Chebyshev II highpass") {
        FilterSpecification spec;
        spec.family = FilterFamily::ChebyshevII;
        spec.response = FilterResponse::Highpass;
        spec.order = 3;
        spec.cutoff_hz = 20.0;
        spec.sampling_rate_hz = 1000.0;
        spec.ripple_db = 1.0;
        spec.zero_phase = true;
        
        auto filter = spec.createFilter();
        REQUIRE(filter);
        REQUIRE(filter->getName().find("Chebyshev II") != std::string::npos);
        
        auto params = AnalogFilterParams::withSpecification(spec);
        auto filtered = filter_analog(series.get(), params);
        REQUIRE(filtered);
        REQUIRE(filtered->getNumSamples() == num_samples);
    }
    
    SECTION("Invalid specification throws on filter creation") {
        FilterSpecification spec;
        spec.order = 10;  // Invalid
        
        REQUIRE_THROWS_AS(spec.createFilter(), std::invalid_argument);
    }
}

TEST_CASE_METHOD(AnalogFilterTestFixture, "Data Transform: Filter - JSON Pipeline Integration", "[transforms][analog_filter][pipeline]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "filter_step_1"},
            {"transform_name", "Filter"},
            {"input_key", "raw_signal"},
            {"output_key", "filtered_signal"},
            {"parameters", {
                {"filter_specification", {
                    {"filter_family", "butterworth"},
                    {"filter_response", "lowpass"},
                    {"order", 4},
                    {"cutoff_hz", 10.0},
                    {"sampling_rate_hz", 1000.0},
                    {"zero_phase", true}
                }}
            }}
        }}}
    };
    
    DataManager dm;
    TransformRegistry registry;
    
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    auto series = m_test_analog_signals["multi_freq_5_50"];
    const size_t num_samples = 2000;
    
    series->setTimeFrame(time_frame);
    dm.setData("raw_signal", series, TimeKey("default"));
    
    TransformPipeline pipeline(&dm, &registry);
    REQUIRE(pipeline.loadFromJson(json_config));
    
    auto result = pipeline.execute();
    REQUIRE(result.success);
    REQUIRE(result.steps_completed == 1);
    
    // Verify the filtered output
    auto filtered_series = dm.getData<AnalogTimeSeries>("filtered_signal");
    REQUIRE(filtered_series != nullptr);
    REQUIRE(filtered_series->getNumSamples() == num_samples);
    
    // The 50 Hz component should be attenuated (cutoff at 10 Hz)
    // Original signal: 1.0 amplitude at 5 Hz + 0.5 amplitude at 50 Hz
    // After 10 Hz lowpass: 5 Hz should pass through, 50 Hz should be heavily attenuated
    auto filtered_data = filtered_series->getAnalogTimeSeries();
    
    // Check that the signal is dominated by the low frequency (5 Hz) component
    // The filtered max amplitude should be close to the original 5 Hz component (1.0)
    // and much less than the original combined signal (would be ~1.5)
    float max_amplitude = 0.0f;
    for (size_t i = 500; i < filtered_data.size(); ++i) {
        max_amplitude = std::max(max_amplitude, std::abs(filtered_data[i]));
    }
    // With 10 Hz lowpass, the output should be dominated by the 5 Hz component
    // Allow some tolerance for passband ripple and phase effects
    REQUIRE(max_amplitude > 0.8f);  // 5 Hz component mostly preserved
    REQUIRE(max_amplitude < 1.3f);  // 50 Hz component significantly attenuated
}

TEST_CASE_METHOD(AnalogFilterTestFixture, "Data Transform: Filter - Multiple Filter Types in Pipeline", "[transforms][analog_filter][pipeline]") {
    const nlohmann::json json_config = {
        {"steps", {
            {
                {"step_id", "lowpass_filter"},
                {"transform_name", "Filter"},
                {"input_key", "raw_signal"},
                {"output_key", "lowpass_signal"},
                {"phase", 0},
                {"parameters", {
                    {"filter_specification", {
                        {"filter_family", "butterworth"},
                        {"filter_response", "lowpass"},
                        {"order", 4},
                        {"cutoff_hz", 50.0},
                        {"sampling_rate_hz", 1000.0},
                        {"zero_phase", false}
                    }}
                }}
            },
            {
                {"step_id", "notch_filter"},
                {"transform_name", "Filter"},
                {"input_key", "lowpass_signal"},
                {"output_key", "notch_signal"},
                {"phase", 1},
                {"parameters", {
                    {"filter_specification", {
                        {"filter_family", "rbj"},
                        {"filter_response", "bandstop"},
                        {"cutoff_hz", 60.0},
                        {"sampling_rate_hz", 1000.0},
                        {"q_factor", 30.0},
                        {"zero_phase", true}
                    }}
                }}
            }
        }}
    };
    
    DataManager dm;
    TransformRegistry registry;
    
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    auto series = m_test_analog_signals["multi_freq_10_60_100"];
    const size_t num_samples = 2000;
    
    series->setTimeFrame(time_frame);
    dm.setData("raw_signal", series, TimeKey("default"));
    
    TransformPipeline pipeline(&dm, &registry);
    REQUIRE(pipeline.loadFromJson(json_config));
    
    auto result = pipeline.execute();
    REQUIRE(result.success);
    REQUIRE(result.steps_completed == 2);
    
    // Verify both outputs exist
    auto lowpass_series = dm.getData<AnalogTimeSeries>("lowpass_signal");
    REQUIRE(lowpass_series != nullptr);
    
    auto notch_series = dm.getData<AnalogTimeSeries>("notch_signal");
    REQUIRE(notch_series != nullptr);
    REQUIRE(notch_series->getNumSamples() == num_samples);
}

TEST_CASE("Data Transform: Filter - Invalid Specification in Pipeline", "[transforms][analog_filter][pipeline]") {
    const nlohmann::json json_config = {
        {"steps", {{
            {"step_id", "invalid_filter"},
            {"transform_name", "Filter"},
            {"input_key", "raw_signal"},
            {"output_key", "filtered_signal"},
            {"parameters", {
                {"filter_specification", {
                    {"filter_family", "butterworth"},
                    {"filter_response", "lowpass"},
                    {"order", 10},  // Invalid order
                    {"cutoff_hz", 10.0},
                    {"sampling_rate_hz", 1000.0}
                }}
            }}
        }}}
    };
    
    DataManager dm;
    TransformRegistry registry;
    
    // The pipeline should fail to load due to invalid filter specification
    TransformPipeline pipeline(&dm, &registry);
    REQUIRE_FALSE(pipeline.loadFromJson(json_config));
}