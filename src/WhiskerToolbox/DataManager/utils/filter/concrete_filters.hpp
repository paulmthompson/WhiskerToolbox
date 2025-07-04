#ifndef CONCRETE_FILTERS_HPP
#define CONCRETE_FILTERS_HPP

#include "new_filter_interface.hpp"
#include "filter.hpp" // For FilterOptions
#include "Iir.h"
#include <stdexcept>

/**
 * @brief Butterworth Low-pass Filter Implementation
 */
template<int Order>
class ButterworthLowPassFilter : public IFilter {
private:
    Iir::Butterworth::LowPass<Order> filter_;
    double sampling_rate_hz_;
    double cutoff_frequency_hz_;

public:
    /**
     * @brief Constructor
     * @param sampling_rate_hz Sampling rate in Hz
     * @param cutoff_frequency_hz Cutoff frequency in Hz
     */
    ButterworthLowPassFilter(double sampling_rate_hz, double cutoff_frequency_hz)
        : sampling_rate_hz_(sampling_rate_hz), cutoff_frequency_hz_(cutoff_frequency_hz) {
        filter_.setup(Order, sampling_rate_hz_, cutoff_frequency_hz_);
    }

    void process(AnalogTimeSeries& data) override {
        auto& analog_data = const_cast<std::vector<float>&>(data.getAnalogTimeSeries());
        
        for (float& sample : analog_data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        filter_.reset();
    }

    std::unique_ptr<IFilter> clone() const override {
        return std::make_unique<ButterworthLowPassFilter<Order>>(sampling_rate_hz_, cutoff_frequency_hz_);
    }
};

/**
 * @brief Butterworth High-pass Filter Implementation
 */
template<int Order>
class ButterworthHighPassFilter : public IFilter {
private:
    Iir::Butterworth::HighPass<Order> filter_;
    double sampling_rate_hz_;
    double cutoff_frequency_hz_;

public:
    ButterworthHighPassFilter(double sampling_rate_hz, double cutoff_frequency_hz)
        : sampling_rate_hz_(sampling_rate_hz), cutoff_frequency_hz_(cutoff_frequency_hz) {
        filter_.setup(Order, sampling_rate_hz_, cutoff_frequency_hz_);
    }

    void process(AnalogTimeSeries& data) override {
        auto& analog_data = const_cast<std::vector<float>&>(data.getAnalogTimeSeries());
        
        for (float& sample : analog_data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        filter_.reset();
    }

    std::unique_ptr<IFilter> clone() const override {
        return std::make_unique<ButterworthHighPassFilter<Order>>(sampling_rate_hz_, cutoff_frequency_hz_);
    }
};

/**
 * @brief Butterworth Band-pass Filter Implementation
 */
template<int Order>
class ButterworthBandPassFilter : public IFilter {
private:
    Iir::Butterworth::BandPass<Order> filter_;
    double sampling_rate_hz_;
    double center_frequency_hz_;
    double bandwidth_hz_;

public:
    ButterworthBandPassFilter(double sampling_rate_hz, double low_cutoff_hz, double high_cutoff_hz)
        : sampling_rate_hz_(sampling_rate_hz) {
        center_frequency_hz_ = (low_cutoff_hz + high_cutoff_hz) / 2.0;
        bandwidth_hz_ = high_cutoff_hz - low_cutoff_hz;
        filter_.setup(Order, sampling_rate_hz_, center_frequency_hz_, bandwidth_hz_);
    }

    void process(AnalogTimeSeries& data) override {
        auto& analog_data = const_cast<std::vector<float>&>(data.getAnalogTimeSeries());
        
        for (float& sample : analog_data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        filter_.reset();
    }

    std::unique_ptr<IFilter> clone() const override {
        return std::make_unique<ButterworthBandPassFilter<Order>>(
            sampling_rate_hz_, center_frequency_hz_ - bandwidth_hz_/2.0, center_frequency_hz_ + bandwidth_hz_/2.0);
    }
};

/**
 * @brief RBJ Notch Filter Implementation
 */
class RBJNotchFilter : public IFilter {
private:
    Iir::RBJ::BandStop filter_;
    double sampling_rate_hz_;
    double center_frequency_hz_;
    double q_factor_;

public:
    RBJNotchFilter(double sampling_rate_hz, double center_frequency_hz, double q_factor)
        : sampling_rate_hz_(sampling_rate_hz), center_frequency_hz_(center_frequency_hz), q_factor_(q_factor) {
        // Convert Q factor to bandwidth in octaves: BW ≈ 1.44 / Q
        double bandwidth_octaves = 1.44 / q_factor_;
        filter_.setup(sampling_rate_hz_, center_frequency_hz_, bandwidth_octaves);
    }

    void process(AnalogTimeSeries& data) override {
        auto& analog_data = const_cast<std::vector<float>&>(data.getAnalogTimeSeries());
        
        for (float& sample : analog_data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        filter_.reset();
    }

    std::unique_ptr<IFilter> clone() const override {
        return std::make_unique<RBJNotchFilter>(sampling_rate_hz_, center_frequency_hz_, q_factor_);
    }
};

#endif // CONCRETE_FILTERS_HPP
