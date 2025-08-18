#include "FilterFactory.hpp"
#include "IFilter.hpp"
#include "ZeroPhaseDecorator.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <iostream>

TEST_CASE("New Filter Interface: Basic Filter Creation", "[filter][new_interface][creation]") {
    SECTION("Create Butterworth lowpass filter") {
        auto filter = FilterFactory::createButterworthLowpass<4>(100.0, 1000.0, false);
        REQUIRE(filter);
        CHECK(filter->getName().find("Butterworth Lowpass Order 4") != std::string::npos);
    }

    SECTION("Create Butterworth highpass filter") {
        auto filter = FilterFactory::createButterworthHighpass<2>(50.0, 1000.0, false);
        REQUIRE(filter);
        CHECK(filter->getName().find("Butterworth Highpass Order 2") != std::string::npos);
    }

    SECTION("Create Butterworth bandpass filter") {
        auto filter = FilterFactory::createButterworthBandpass<4>(50.0, 150.0, 1000.0, false);
        REQUIRE(filter);
        CHECK(filter->getName().find("Butterworth Bandpass Order 4") != std::string::npos);
    }
}

TEST_CASE("New Filter Interface: Zero Phase Decorator", "[filter][new_interface][zero_phase]") {
    SECTION("Zero phase lowpass filter creation") {
        auto filter = FilterFactory::createButterworthLowpass<4>(100.0, 1000.0, true);
        REQUIRE(filter);
        CHECK(filter->getName().find("ZeroPhase") != std::string::npos);
        CHECK(filter->getName().find("Butterworth Lowpass") != std::string::npos);
    }

    SECTION("Manual zero phase decorator creation") {
        auto base_filter = FilterFactory::createButterworthLowpass<4>(100.0, 1000.0, false);
        auto zero_phase_filter = std::make_unique<ZeroPhaseDecorator>(std::move(base_filter));
        
        REQUIRE(zero_phase_filter);
        CHECK(zero_phase_filter->getName().find("ZeroPhase") != std::string::npos);
    }
}

TEST_CASE("New Filter Interface: Filter Processing", "[filter][new_interface][processing]") {
    // Create test data: sine wave at 10 Hz sampled at 1000 Hz
    const size_t num_samples = 1000;
    const double sampling_rate = 1000.0;
    const double signal_freq = 10.0;
    
    std::vector<float> test_data;
    test_data.reserve(num_samples);
    
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        test_data.push_back(static_cast<float>(std::sin(2.0 * M_PI * signal_freq * t)));
    }

    SECTION("Basic lowpass filtering") {
        auto filter = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, false);
        
        // Make a copy for processing
        auto filtered_data = test_data;
        std::span<float> data_span(filtered_data);
        
        // Process the data
        filter->process(data_span);
        
        // Check that data was modified
        bool data_changed = false;
        for (size_t i = 0; i < num_samples; ++i) {
            if (std::abs(filtered_data[i] - test_data[i]) > 0.001f) {
                data_changed = true;
                break;
            }
        }
        REQUIRE(data_changed);
    }

    SECTION("Zero phase vs regular filtering comparison") {
        auto regular_filter = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, false);
        auto zero_phase_filter = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, true);
        
        auto regular_data = test_data;
        auto zero_phase_data = test_data;
        
        regular_filter->process(std::span<float>(regular_data));
        zero_phase_filter->process(std::span<float>(zero_phase_data));
        
        // Both should change the data, but differently
        bool regular_changed = false;
        bool zero_phase_changed = false;
        bool filters_differ = false;
        
        for (size_t i = 0; i < num_samples; ++i) {
            if (std::abs(regular_data[i] - test_data[i]) > 0.001f) {
                regular_changed = true;
            }
            if (std::abs(zero_phase_data[i] - test_data[i]) > 0.001f) {
                zero_phase_changed = true;
            }
            if (std::abs(regular_data[i] - zero_phase_data[i]) > 0.001f) {
                filters_differ = true;
            }
        }
        
        REQUIRE(regular_changed);
        REQUIRE(zero_phase_changed);
        REQUIRE(filters_differ);
    }

    SECTION("Filter reset functionality") {
        auto filter = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, false);
        
        // Process some data
        auto data1 = test_data;
        filter->process(std::span<float>(data1));
        
        // Reset and process same data again
        filter->reset();
        auto data2 = test_data;
        filter->process(std::span<float>(data2));
        
        // Results should be identical
        for (size_t i = 0; i < num_samples; ++i) {
            CHECK(data1[i] == Catch::Approx(data2[i]).epsilon(1e-6));
        }
    }
}


TEST_CASE("New Filter Interface: Advanced Functionality", "[filter][new_interface][advanced]") {
    const size_t num_samples = 1000;
    const double sampling_rate = 1000.0;
    
    // Create test data: sine wave at 10 Hz
    std::vector<float> test_data;
    test_data.reserve(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        test_data.push_back(static_cast<float>(std::sin(2.0 * M_PI * 10.0 * t)));
    }

    SECTION("Direct filter processing") {
        auto filter = FilterFactory::createButterworthLowpass<4>(100.0, sampling_rate, false);
        REQUIRE(filter);
        
        auto data_copy = test_data;
        std::span<float> data_span(data_copy);
        REQUIRE_NOTHROW(filter->process(data_span));
        
        // Verify data was modified
        bool changed = false;
        for (size_t i = 0; i < num_samples; ++i) {
            if (std::abs(data_copy[i] - test_data[i]) > 0.001f) {
                changed = true;
                break;
            }
        }
        REQUIRE(changed);
    }

    SECTION("Multiple filter types") {
        auto lowpass = FilterFactory::createButterworthLowpass<4>(100.0, sampling_rate, false);
        auto highpass = FilterFactory::createButterworthHighpass<4>(50.0, sampling_rate, false);
        auto bandpass = FilterFactory::createButterworthBandpass<4>(50.0, 150.0, sampling_rate, false);
        
        REQUIRE(lowpass);
        REQUIRE(highpass);
        REQUIRE(bandpass);
        
        auto lp_data = test_data;
        auto hp_data = test_data;
        auto bp_data = test_data;
        
        REQUIRE_NOTHROW(lowpass->process(std::span<float>(lp_data)));
        REQUIRE_NOTHROW(highpass->process(std::span<float>(hp_data)));
        REQUIRE_NOTHROW(bandpass->process(std::span<float>(bp_data)));
    }
}

TEST_CASE("New Filter Interface: Chebyshev Filters", "[filter][new_interface][chebyshev]") {
    SECTION("Create Chebyshev I lowpass filter") {
        auto filter = FilterFactory::createChebyshevILowpass<4>(100.0, 1000.0, 1.0, false);
        REQUIRE(filter);
        CHECK(filter->getName().find("Chebyshev I Lowpass Order 4") != std::string::npos);
        CHECK(filter->getName().find("ripple=1") != std::string::npos);
    }

    SECTION("Create Chebyshev I highpass filter") {
        auto filter = FilterFactory::createChebyshevIHighpass<2>(50.0, 1000.0, 0.5, false);
        REQUIRE(filter);
        CHECK(filter->getName().find("Chebyshev I Highpass Order 2") != std::string::npos);
        CHECK(filter->getName().find("ripple=0.5") != std::string::npos);
    }

    SECTION("Zero phase Chebyshev filter") {
        auto filter = FilterFactory::createChebyshevILowpass<4>(100.0, 1000.0, 1.0, true);
        REQUIRE(filter);
        CHECK(filter->getName().find("ZeroPhase") != std::string::npos);
        CHECK(filter->getName().find("Chebyshev I Lowpass") != std::string::npos);
    }
}

TEST_CASE("New Filter Interface: Chebyshev Processing", "[filter][new_interface][chebyshev][processing]") {
    // Create test data: sine wave at 10 Hz sampled at 1000 Hz
    const size_t num_samples = 1000;
    const double sampling_rate = 1000.0;
    const double signal_freq = 10.0;
    
    std::vector<float> test_data;
    test_data.reserve(num_samples);
    
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        test_data.push_back(static_cast<float>(std::sin(2.0 * M_PI * signal_freq * t)));
    }

    SECTION("Chebyshev lowpass filtering") {
        auto filter = FilterFactory::createChebyshevILowpass<4>(50.0, sampling_rate, 1.0, false);
        
        auto filtered_data = test_data;
        std::span<float> data_span(filtered_data);
        
        REQUIRE_NOTHROW(filter->process(data_span));
        
        // Check that data was modified
        bool data_changed = false;
        for (size_t i = 0; i < num_samples; ++i) {
            if (std::abs(filtered_data[i] - test_data[i]) > 0.001f) {
                data_changed = true;
                break;
            }
        }
        REQUIRE(data_changed);
    }

    SECTION("Compare Butterworth vs Chebyshev response") {
        auto butterworth_filter = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, false);
        auto chebyshev_filter = FilterFactory::createChebyshevILowpass<4>(50.0, sampling_rate, 1.0, false);
        
        auto butterworth_data = test_data;
        auto chebyshev_data = test_data;
        
        butterworth_filter->process(std::span<float>(butterworth_data));
        chebyshev_filter->process(std::span<float>(chebyshev_data));
        
        // Filters should produce different results
        bool filters_differ = false;
        for (size_t i = 100; i < num_samples; ++i) { // Skip transient
            if (std::abs(butterworth_data[i] - chebyshev_data[i]) > 0.001f) {
                filters_differ = true;
                break;
            }
        }
        REQUIRE(filters_differ);
    }
}

TEST_CASE("New Filter Interface: Complete Filter Library", "[filter][new_interface][complete]") {
    const double sampling_rate = 1000.0;
    const size_t num_samples = 1000;
    
    // Create test data: sine wave at 10 Hz
    std::vector<float> test_data;
    test_data.reserve(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sampling_rate;
        test_data.push_back(static_cast<float>(std::sin(2.0 * M_PI * 10.0 * t)));
    }

    SECTION("All Butterworth filter types") {
        auto lowpass = FilterFactory::createButterworthLowpass<4>(50.0, sampling_rate, false);
        auto highpass = FilterFactory::createButterworthHighpass<4>(20.0, sampling_rate, false);
        auto bandpass = FilterFactory::createButterworthBandpass<4>(8.0, 12.0, sampling_rate, false);
        auto bandstop = FilterFactory::createButterworthBandstop<4>(8.0, 12.0, sampling_rate, false);
        
        REQUIRE(lowpass);
        REQUIRE(highpass);
        REQUIRE(bandpass);
        REQUIRE(bandstop);
        
        // Test processing
        auto lp_data = test_data;
        auto hp_data = test_data;
        auto bp_data = test_data;
        auto bs_data = test_data;
        
        REQUIRE_NOTHROW(lowpass->process(std::span<float>(lp_data)));
        REQUIRE_NOTHROW(highpass->process(std::span<float>(hp_data)));
        REQUIRE_NOTHROW(bandpass->process(std::span<float>(bp_data)));
        REQUIRE_NOTHROW(bandstop->process(std::span<float>(bs_data)));
    }

    SECTION("All Chebyshev I filter types") {
        auto lowpass = FilterFactory::createChebyshevILowpass<4>(50.0, sampling_rate, 1.0, false);
        auto highpass = FilterFactory::createChebyshevIHighpass<4>(20.0, sampling_rate, 1.0, false);
        auto bandpass = FilterFactory::createChebyshevIBandpass<4>(8.0, 12.0, sampling_rate, 1.0, false);
        auto bandstop = FilterFactory::createChebyshevIBandstop<4>(8.0, 12.0, sampling_rate, 1.0, false);
        
        REQUIRE(lowpass);
        REQUIRE(highpass);
        REQUIRE(bandpass);
        REQUIRE(bandstop);
        
        CHECK(lowpass->getName().find("Chebyshev I") != std::string::npos);
        CHECK(highpass->getName().find("Chebyshev I") != std::string::npos);
        CHECK(bandpass->getName().find("Chebyshev I") != std::string::npos);
        CHECK(bandstop->getName().find("Chebyshev I") != std::string::npos);
    }

    SECTION("All Chebyshev II filter types") {
        auto lowpass = FilterFactory::createChebyshevIILowpass<4>(50.0, sampling_rate, 20.0, false);
        auto highpass = FilterFactory::createChebyshevIIHighpass<4>(20.0, sampling_rate, 20.0, false);
        auto bandpass = FilterFactory::createChebyshevIIBandpass<4>(8.0, 12.0, sampling_rate, 20.0, false);
        auto bandstop = FilterFactory::createChebyshevIIBandstop<4>(8.0, 12.0, sampling_rate, 20.0, false);
        
        REQUIRE(lowpass);
        REQUIRE(highpass);
        REQUIRE(bandpass);
        REQUIRE(bandstop);
        
        CHECK(lowpass->getName().find("Chebyshev II") != std::string::npos);
        CHECK(highpass->getName().find("Chebyshev II") != std::string::npos);
        CHECK(bandpass->getName().find("Chebyshev II") != std::string::npos);
        CHECK(bandstop->getName().find("Chebyshev II") != std::string::npos);
    }

    SECTION("All RBJ filter types") {
        auto lowpass = FilterFactory::createRBJLowpass(50.0, sampling_rate, 0.707, false);
        auto highpass = FilterFactory::createRBJHighpass(20.0, sampling_rate, 0.707, false);
        auto bandpass = FilterFactory::createRBJBandpass(10.0, sampling_rate, 1.0, false);
        auto bandstop = FilterFactory::createRBJBandstop(10.0, sampling_rate, 10.0, false);
        
        REQUIRE(lowpass);
        REQUIRE(highpass);
        REQUIRE(bandpass);
        REQUIRE(bandstop);
        
        CHECK(lowpass->getName().find("RBJ") != std::string::npos);
        CHECK(highpass->getName().find("RBJ") != std::string::npos);
        CHECK(bandpass->getName().find("RBJ") != std::string::npos);
        CHECK(bandstop->getName().find("RBJ") != std::string::npos);
    }
}
