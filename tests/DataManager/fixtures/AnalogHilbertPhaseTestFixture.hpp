#ifndef ANALOG_HILBERT_PHASE_TEST_FIXTURE_HPP
#define ANALOG_HILBERT_PHASE_TEST_FIXTURE_HPP

#include "catch2/catch_test_macros.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <numbers>

class AnalogHilbertPhaseTestFixture {
protected:
    AnalogHilbertPhaseTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        populateTestData();
    }

    ~AnalogHilbertPhaseTestFixture() = default;

    DataManager* getDataManager() {
        return m_data_manager.get();
    }

    std::map<std::string, std::shared_ptr<AnalogTimeSeries>> m_test_analog_signals;
    std::map<std::string, std::shared_ptr<TimeFrame>> m_test_timeframes;

private:
    void populateTestData() {
        // Simple sine wave - 1 Hz, 100 Hz sampling, 200 samples
        createSineWaveSignal("sine_1hz_200", 200, 100.0, 1.0f);

        // Cosine wave - 2 Hz, 50 Hz sampling, 100 samples
        createCosineWaveSignal("cosine_2hz_100", 100, 50.0, 2.0f);

        // Complex signal with multiple frequencies - 2Hz and 5Hz components
        createMultiFrequencySignal("multi_freq_2_5", 300, 100.0,
                                  {{2.0f, 1.0f}, {5.0f, 0.5f}});

        // Discontinuous time series with large gaps
        createDiscontinuousSignal("discontinuous_large_gap", 
                                 {1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f},
                                 {0, 1, 2, 3, 2000, 2001, 2002, 2003});

        // Multiple discontinuities
        createDiscontinuousSignal("multiple_discontinuities",
                                 {1.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f},
                                 {0, 1, 2, 1000, 1001, 2000});

        // Simple signal for progress callback testing
        createDiscontinuousSignal("progress_callback_signal",
                                 {1.0f, 0.0f, -1.0f, 0.0f, 1.0f},
                                 {0, 25, 50, 75, 100});

        // Default parameters test signal
        createDiscontinuousSignal("default_params_signal",
                                 {1.0f, 2.0f, 1.0f, 0.0f, -1.0f},
                                 {0, 10, 20, 30, 40});

        // Amplitude extraction - simple sine wave with known amplitude (2.5)
        createSineWaveSignal("amplitude_sine_2_5", 200, 100.0, 1.0f, 2.5f);

        // Amplitude modulated signal
        createAmplitudeModulatedSignal("amplitude_modulated", 200, 100.0, 10.0f, 1.0f);

        // Amplitude extraction with discontinuities
        createDiscontinuousSignal("amplitude_discontinuous",
                                 {1.0f, 0.5f, -1.0f, 0.5f, 2.0f, 1.0f, -2.0f, 1.0f},
                                 {0, 1, 2, 3, 2000, 2001, 2002, 2003});

        // Long signal for windowed processing - 150000 samples
        createSineWaveSignal("long_sine_5hz", 150000, 1000.0, 5.0f, 2.0f);

        // Empty signal
        createDiscontinuousSignal("empty_signal", {}, {});

        // Single sample
        createDiscontinuousSignal("single_sample", {1.0f}, {0});

        // Invalid frequency parameters signal
        createDiscontinuousSignal("invalid_freq_params",
                                 {1.0f, 0.0f, -1.0f, 0.0f},
                                 {0, 25, 50, 75});

        // Time series with NaN values
        std::vector<float> nan_values = {1.0f, std::numeric_limits<float>::quiet_NaN(), -1.0f, 0.0f};
        createDiscontinuousSignal("signal_with_nan", nan_values, {0, 25, 50, 75});

        // Irregular timestamp spacing
        createDiscontinuousSignal("irregular_spacing",
                                 {1.0f, 0.0f, -1.0f, 0.0f, 1.0f},
                                 {0, 1, 10, 11, 100});

        // Very small discontinuity threshold test
        createDiscontinuousSignal("small_gaps",
                                 {1.0f, 0.0f, -1.0f, 0.0f},
                                 {0, 5, 10, 15});

        // Very large discontinuity threshold test
        createDiscontinuousSignal("large_gaps",
                                 {1.0f, 0.0f, -1.0f, 0.0f},
                                 {0, 100, 200, 300});

        // Irregularly sampled data for interpolation testing
        createIrregularlySampledSignal("irregularly_sampled", 1000.0, 10.0);

        // Pipeline test signal - 10 Hz sine wave, 100 Hz sampling, 200 samples
        createSineWaveSignal("pipeline_test_signal", 200, 100.0, 10.0f);
    }

    void createSineWaveSignal(const std::string& key, size_t num_samples,
                             double sampling_rate, float frequency, float amplitude = 1.0f) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;

        data.reserve(num_samples);
        times.reserve(num_samples);
        timeframe_values.reserve(num_samples);

        for (size_t i = 0; i < num_samples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(sampling_rate);
            data.push_back(amplitude * std::sin(2.0f * std::numbers::pi_v<float> * frequency * t));
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }

        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);

        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createCosineWaveSignal(const std::string& key, size_t num_samples,
                               double sampling_rate, float frequency, float amplitude = 1.0f) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;

        data.reserve(num_samples);
        times.reserve(num_samples);
        timeframe_values.reserve(num_samples);

        for (size_t i = 0; i < num_samples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(sampling_rate);
            data.push_back(amplitude * std::cos(2.0f * std::numbers::pi_v<float> * frequency * t));
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }

        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);

        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createMultiFrequencySignal(const std::string& key, size_t num_samples,
                                   double sampling_rate,
                                   const std::vector<std::pair<float, float>>& freq_amp_pairs) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;

        data.reserve(num_samples);
        times.reserve(num_samples);
        timeframe_values.reserve(num_samples);

        for (size_t i = 0; i < num_samples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(sampling_rate);
            float value = 0.0f;

            for (const auto& [freq, amp] : freq_amp_pairs) {
                value += amp * std::sin(2.0f * std::numbers::pi_v<float> * freq * t);
            }

            data.push_back(value);
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }

        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);

        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createDiscontinuousSignal(const std::string& key,
                                   const std::vector<float>& values,
                                   const std::vector<int>& time_indices) {
        std::vector<TimeFrameIndex> times;
        times.reserve(time_indices.size());

        for (int idx : time_indices) {
            times.push_back(TimeFrameIndex(idx));
        }

        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        
        // For discontinuous signals, create a minimal timeframe
        if (!time_indices.empty()) {
            std::vector<int> timeframe_values;
            for (int idx : time_indices) {
                timeframe_values.push_back(idx);
            }
            auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
            ats->setTimeFrame(time_frame);
            m_test_timeframes[key + "_tf"] = time_frame;
        }

        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
    }

    void createAmplitudeModulatedSignal(const std::string& key, size_t num_samples,
                                       double sampling_rate, float carrier_freq,
                                       float modulation_freq) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;
        std::vector<int> timeframe_values;

        data.reserve(num_samples);
        times.reserve(num_samples);
        timeframe_values.reserve(num_samples);

        for (size_t i = 0; i < num_samples; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(sampling_rate);
            float envelope = 1.0f + 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * modulation_freq * t);
            float carrier = std::sin(2.0f * std::numbers::pi_v<float> * carrier_freq * t);
            data.push_back(envelope * carrier);
            times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
            timeframe_values.push_back(static_cast<int>(i));
        }

        auto ats = std::make_shared<AnalogTimeSeries>(data, times);
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);

        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    void createIrregularlySampledSignal(const std::string& key,
                                       double sampling_rate, double frequency) {
        std::vector<float> data;
        std::vector<TimeFrameIndex> times;

        // First segment: points at 0,1,3,4,6,7,9,10 (skipping 2,5,8)
        for (int i = 0; i <= 10; i++) {
            if (i % 3 == 2) continue; // Skip every third point

            double t = i / sampling_rate;
            data.push_back(static_cast<float>(std::sin(2.0 * M_PI * frequency * t)));
            times.push_back(TimeFrameIndex(i));
        }

        // Large gap (100 samples)

        // Second segment: points at 110,111,113,114,116,117,119,120
        for (int i = 110; i <= 120; i++) {
            if (i % 3 == 2) continue; // Skip every third point

            double t = i / sampling_rate;
            data.push_back(static_cast<float>(std::sin(2.0 * M_PI * frequency * t)));
            times.push_back(TimeFrameIndex(i));
        }

        auto ats = std::make_shared<AnalogTimeSeries>(data, times);

        // Create a minimal timeframe for the existing points
        std::vector<int> timeframe_values;
        for (const auto& time_idx : times) {
            timeframe_values.push_back(static_cast<int>(time_idx.getValue()));
        }
        auto time_frame = std::make_shared<TimeFrame>(timeframe_values);
        ats->setTimeFrame(time_frame);

        m_data_manager->setData(key, ats, TimeKey(key + "_time"));
        m_test_analog_signals[key] = ats;
        m_test_timeframes[key + "_tf"] = time_frame;
    }

    std::unique_ptr<DataManager> m_data_manager;
};

#endif // ANALOG_HILBERT_PHASE_TEST_FIXTURE_HPP
