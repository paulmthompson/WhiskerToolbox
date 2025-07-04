#ifndef FILTER_IMPLEMENTATIONS_HPP
#define FILTER_IMPLEMENTATIONS_HPP

#include "IFilter.hpp"
#include "Iir.h"
#include <memory>
#include <sstream>
#include <stdexcept>

/**
 * @brief Butterworth Low-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ButterworthLowpassFilter : public IFilter {
private:
    Iir::Butterworth::LowPass<Order> filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    bool configured_ = false;

public:
    /**
     * @brief Construct and configure the filter
     * 
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     */
    ButterworthLowpassFilter(double cutoff_hz, double sampling_rate_hz) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz) {
        try {
            filter_.setup(Order, sampling_rate_hz_, cutoff_hz_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Butterworth lowpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        // Process all samples - the tight loop is inside the virtual function
        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Butterworth Lowpass Order " << Order << " (fc=" << cutoff_hz_ << "Hz)";
        return oss.str();
    }
};

/**
 * @brief Butterworth High-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ButterworthHighpassFilter : public IFilter {
private:
    Iir::Butterworth::HighPass<Order> filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    bool configured_ = false;

public:
    ButterworthHighpassFilter(double cutoff_hz, double sampling_rate_hz) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz) {
        try {
            filter_.setup(Order, sampling_rate_hz_, cutoff_hz_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Butterworth highpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Butterworth Highpass Order " << Order << " (fc=" << cutoff_hz_ << "Hz)";
        return oss.str();
    }
};

/**
 * @brief Butterworth Band-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ButterworthBandpassFilter : public IFilter {
private:
    Iir::Butterworth::BandPass<Order> filter_;
    double low_cutoff_hz_;
    double high_cutoff_hz_;
    double sampling_rate_hz_;
    bool configured_ = false;

public:
    ButterworthBandpassFilter(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz) 
        : low_cutoff_hz_(low_cutoff_hz), high_cutoff_hz_(high_cutoff_hz), sampling_rate_hz_(sampling_rate_hz) {
        try {
            double center_freq = (low_cutoff_hz_ + high_cutoff_hz_) / 2.0;
            double bandwidth = high_cutoff_hz_ - low_cutoff_hz_;
            filter_.setup(Order, sampling_rate_hz_, center_freq, bandwidth);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Butterworth bandpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Butterworth Bandpass Order " << Order << " (fc=" << low_cutoff_hz_ << "-" << high_cutoff_hz_ << "Hz)";
        return oss.str();
    }
};

/**
 * @brief Butterworth Band-stop filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ButterworthBandstopFilter : public IFilter {
private:
    Iir::Butterworth::BandStop<Order> filter_;
    double low_cutoff_hz_;
    double high_cutoff_hz_;
    double sampling_rate_hz_;
    bool configured_ = false;

public:
    ButterworthBandstopFilter(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz) 
        : low_cutoff_hz_(low_cutoff_hz), high_cutoff_hz_(high_cutoff_hz), sampling_rate_hz_(sampling_rate_hz) {
        try {
            double center_freq = (low_cutoff_hz_ + high_cutoff_hz_) / 2.0;
            double bandwidth = high_cutoff_hz_ - low_cutoff_hz_;
            filter_.setup(sampling_rate_hz_, center_freq, bandwidth);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Butterworth bandstop filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Butterworth Bandstop Order " << Order << " (fc=" << low_cutoff_hz_ << "-" << high_cutoff_hz_ << "Hz)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev Type I Low-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevILowpassFilter : public IFilter {
private:
    Iir::ChebyshevI::LowPass<Order> filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    double passband_ripple_db_;
    bool configured_ = false;

public:
    /**
     * @brief Construct and configure the filter
     * 
     * @param cutoff_hz Cutoff frequency in Hz
     * @param sampling_rate_hz Sampling rate in Hz
     * @param passband_ripple_db Passband ripple in dB
     */
    ChebyshevILowpassFilter(double cutoff_hz, double sampling_rate_hz, double passband_ripple_db = 1.0) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz), passband_ripple_db_(passband_ripple_db) {
        try {
            filter_.setup(Order, sampling_rate_hz_, cutoff_hz_, passband_ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev I lowpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev I Lowpass Order " << Order << " (fc=" << cutoff_hz_ << "Hz, ripple=" << passband_ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev Type I High-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevIHighpassFilter : public IFilter {
private:
    Iir::ChebyshevI::HighPass<Order> filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    double passband_ripple_db_;
    bool configured_ = false;

public:
    ChebyshevIHighpassFilter(double cutoff_hz, double sampling_rate_hz, double passband_ripple_db = 1.0) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz), passband_ripple_db_(passband_ripple_db) {
        try {
            filter_.setup(Order, sampling_rate_hz_, cutoff_hz_, passband_ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev I highpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev I Highpass Order " << Order << " (fc=" << cutoff_hz_ << "Hz, ripple=" << passband_ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev Type I Band-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevIBandpassFilter : public IFilter {
private:
    Iir::ChebyshevI::BandPass<Order> filter_;
    double low_cutoff_hz_;
    double high_cutoff_hz_;
    double sampling_rate_hz_;
    double ripple_db_;
    bool configured_ = false;

public:
    ChebyshevIBandpassFilter(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db) 
        : low_cutoff_hz_(low_cutoff_hz), high_cutoff_hz_(high_cutoff_hz), sampling_rate_hz_(sampling_rate_hz), ripple_db_(ripple_db) {
        try {
            double center_freq = (low_cutoff_hz_ + high_cutoff_hz_) / 2.0;
            double bandwidth = high_cutoff_hz_ - low_cutoff_hz_;
            filter_.setup(Order, sampling_rate_hz_, center_freq, bandwidth, ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev I bandpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev I Bandpass Order " << Order << " (fc=" << low_cutoff_hz_ << "-" << high_cutoff_hz_ << "Hz, ripple=" << ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev Type I Band-stop filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevIBandstopFilter : public IFilter {
private:
    Iir::ChebyshevI::BandStop<Order> filter_;
    double low_cutoff_hz_;
    double high_cutoff_hz_;
    double sampling_rate_hz_;
    double ripple_db_;
    bool configured_ = false;

public:
    ChebyshevIBandstopFilter(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db) 
        : low_cutoff_hz_(low_cutoff_hz), high_cutoff_hz_(high_cutoff_hz), sampling_rate_hz_(sampling_rate_hz), ripple_db_(ripple_db) {
        try {
            double center_freq = (low_cutoff_hz_ + high_cutoff_hz_) / 2.0;
            double bandwidth = high_cutoff_hz_ - low_cutoff_hz_;
            filter_.setup(Order, sampling_rate_hz_, center_freq, bandwidth, ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev I bandstop filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev I Bandstop Order " << Order << " (fc=" << low_cutoff_hz_ << "-" << high_cutoff_hz_ << "Hz, ripple=" << ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev II Low-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevIILowpassFilter : public IFilter {
private:
    Iir::ChebyshevII::LowPass<Order> filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    double stopband_ripple_db_;
    bool configured_ = false;

public:
    ChebyshevIILowpassFilter(double cutoff_hz, double sampling_rate_hz, double stopband_ripple_db) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz), stopband_ripple_db_(stopband_ripple_db) {
        try {
            filter_.setup(Order, sampling_rate_hz_, cutoff_hz_, stopband_ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev II lowpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev II Lowpass Order " << Order << " (fc=" << cutoff_hz_ << "Hz, stopband=" << stopband_ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev II High-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevIIHighpassFilter : public IFilter {
private:
    Iir::ChebyshevII::HighPass<Order> filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    double stopband_ripple_db_;
    bool configured_ = false;

public:
    ChebyshevIIHighpassFilter(double cutoff_hz, double sampling_rate_hz, double stopband_ripple_db) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz), stopband_ripple_db_(stopband_ripple_db) {
        try {
            filter_.setup(Order, sampling_rate_hz_, cutoff_hz_, stopband_ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev II highpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev II Highpass Order " << Order << " (fc=" << cutoff_hz_ << "Hz, stopband=" << stopband_ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev II Band-pass filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevIIBandpassFilter : public IFilter {
private:
    Iir::ChebyshevII::BandPass<Order> filter_;
    double low_cutoff_hz_;
    double high_cutoff_hz_;
    double sampling_rate_hz_;
    double stopband_ripple_db_;
    bool configured_ = false;

public:
    ChebyshevIIBandpassFilter(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double stopband_ripple_db) 
        : low_cutoff_hz_(low_cutoff_hz), high_cutoff_hz_(high_cutoff_hz), sampling_rate_hz_(sampling_rate_hz), stopband_ripple_db_(stopband_ripple_db) {
        try {
            double center_freq = (low_cutoff_hz_ + high_cutoff_hz_) / 2.0;
            double bandwidth = high_cutoff_hz_ - low_cutoff_hz_;
            filter_.setup(Order, sampling_rate_hz_, center_freq, bandwidth, stopband_ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev II bandpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev II Bandpass Order " << Order << " (fc=" << low_cutoff_hz_ << "-" << high_cutoff_hz_ << "Hz, stopband=" << stopband_ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief Chebyshev II Band-stop filter implementation
 * 
 * @tparam Order The filter order (must be known at compile time)
 */
template<int Order>
class ChebyshevIIBandstopFilter : public IFilter {
private:
    Iir::ChebyshevII::BandStop<Order> filter_;
    double low_cutoff_hz_;
    double high_cutoff_hz_;
    double sampling_rate_hz_;
    double stopband_ripple_db_;
    bool configured_ = false;

public:
    ChebyshevIIBandstopFilter(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double stopband_ripple_db) 
        : low_cutoff_hz_(low_cutoff_hz), high_cutoff_hz_(high_cutoff_hz), sampling_rate_hz_(sampling_rate_hz), stopband_ripple_db_(stopband_ripple_db) {
        try {
            double center_freq = (low_cutoff_hz_ + high_cutoff_hz_) / 2.0;
            double bandwidth = high_cutoff_hz_ - low_cutoff_hz_;
            filter_.setup(Order, sampling_rate_hz_, center_freq, bandwidth, stopband_ripple_db_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure Chebyshev II bandstop filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "Chebyshev II Bandstop Order " << Order << " (fc=" << low_cutoff_hz_ << "-" << high_cutoff_hz_ << "Hz, stopband=" << stopband_ripple_db_ << "dB)";
        return oss.str();
    }
};

/**
 * @brief RBJ Low-pass filter implementation (always 2nd order)
 */
class RBJLowpassFilter : public IFilter {
private:
    Iir::RBJ::LowPass filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    double q_factor_;
    bool configured_ = false;

public:
    RBJLowpassFilter(double cutoff_hz, double sampling_rate_hz, double q_factor) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz), q_factor_(q_factor) {
        try {
            filter_.setup(sampling_rate_hz_, cutoff_hz_, q_factor_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure RBJ lowpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "RBJ Lowpass (fc=" << cutoff_hz_ << "Hz, Q=" << q_factor_ << ")";
        return oss.str();
    }
};

/**
 * @brief RBJ High-pass filter implementation (always 2nd order)
 */
class RBJHighpassFilter : public IFilter {
private:
    Iir::RBJ::HighPass filter_;
    double cutoff_hz_;
    double sampling_rate_hz_;
    double q_factor_;
    bool configured_ = false;

public:
    RBJHighpassFilter(double cutoff_hz, double sampling_rate_hz, double q_factor) 
        : cutoff_hz_(cutoff_hz), sampling_rate_hz_(sampling_rate_hz), q_factor_(q_factor) {
        try {
            filter_.setup(sampling_rate_hz_, cutoff_hz_, q_factor_);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure RBJ highpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "RBJ Highpass (fc=" << cutoff_hz_ << "Hz, Q=" << q_factor_ << ")";
        return oss.str();
    }
};

/**
 * @brief RBJ Band-pass filter implementation (always 2nd order)
 */
class RBJBandpassFilter : public IFilter {
private:
    Iir::RBJ::BandPass2 filter_;
    double center_freq_hz_;
    double sampling_rate_hz_;
    double q_factor_;
    bool configured_ = false;

public:
    // Constructor for Q-factor based design (standard approach for RBJ)
    RBJBandpassFilter(double center_freq_hz, double sampling_rate_hz, double q_factor) 
        : center_freq_hz_(center_freq_hz), sampling_rate_hz_(sampling_rate_hz), q_factor_(q_factor) {
        try {
            // Convert Q factor to bandwidth in octaves: BW ≈ 1.44 / Q
            double bandwidth_octaves = 1.44 / q_factor_;
            filter_.setup(sampling_rate_hz_, center_freq_hz_, bandwidth_octaves);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure RBJ bandpass filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "RBJ Bandpass (fc=" << center_freq_hz_ << "Hz, Q=" << q_factor_ << ")";
        return oss.str();
    }
};

/**
 * @brief RBJ Band-stop/Notch filter implementation (always 2nd order)
 */
class RBJBandstopFilter : public IFilter {
private:
    Iir::RBJ::BandStop filter_;
    double center_freq_hz_;
    double sampling_rate_hz_;
    double q_factor_;
    bool configured_ = false;

public:
    // Constructor for Q-factor based notch filter design (standard approach for RBJ)
    RBJBandstopFilter(double center_freq_hz, double sampling_rate_hz, double q_factor) 
        : center_freq_hz_(center_freq_hz), sampling_rate_hz_(sampling_rate_hz), q_factor_(q_factor) {
        try {
            // Convert Q factor to bandwidth in octaves: BW ≈ 1.44 / Q
            double bandwidth_octaves = 1.44 / q_factor_;
            filter_.setup(sampling_rate_hz_, center_freq_hz_, bandwidth_octaves);
            configured_ = true;
        } catch (std::exception const& e) {
            throw std::runtime_error("Failed to configure RBJ bandstop filter: " + std::string(e.what()));
        }
    }

    void process(std::span<float> data) override {
        if (!configured_) {
            throw std::runtime_error("Filter not properly configured");
        }

        for (float& sample : data) {
            sample = static_cast<float>(filter_.filter(sample));
        }
    }

    void reset() override {
        if (configured_) {
            filter_.reset();
        }
    }

    std::string getName() const override {
        std::ostringstream oss;
        oss << "RBJ Bandstop/Notch (fc=" << center_freq_hz_ << "Hz, Q=" << q_factor_ << ")";
        return oss.str();
    }
};
#endif // FILTER_IMPLEMENTATIONS_HPP
