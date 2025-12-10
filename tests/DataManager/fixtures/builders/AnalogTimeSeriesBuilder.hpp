#ifndef ANALOG_TIME_SERIES_BUILDER_HPP
#define ANALOG_TIME_SERIES_BUILDER_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <fstream>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Lightweight builder for AnalogTimeSeries objects
 * 
 * Provides fluent API for constructing AnalogTimeSeries test data
 * with common waveforms and patterns, without requiring DataManager.
 * 
 * @example Simple signal
 * @code
 * auto signal = AnalogTimeSeriesBuilder()
 *     .withValues({1.0f, 2.0f, 3.0f})
 *     .atTimes({0, 10, 20})
 *     .build();
 * @endcode
 * 
 * @example Sine wave
 * @code
 * auto signal = AnalogTimeSeriesBuilder()
 *     .withSineWave(0, 100, 10.0f)
 *     .build();
 * @endcode
 */
class AnalogTimeSeriesBuilder {
public:
    AnalogTimeSeriesBuilder() = default;

    /**
     * @brief Specify values explicitly
     * @param values Vector of analog values
     */
    AnalogTimeSeriesBuilder& withValues(std::vector<float> values) {
        m_values = std::move(values);
        return *this;
    }

    /**
     * @brief Specify time indices explicitly
     * @param times Vector of time indices (must match values size)
     */
    AnalogTimeSeriesBuilder& atTimes(std::vector<int> times) {
        m_time_indices.clear();
        for (int t : times) {
            m_time_indices.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Use sequential time indices starting from 0
     * Creates indices {0, 1, 2, ...} matching the number of values
     */
    AnalogTimeSeriesBuilder& withSequentialTimes() {
        m_time_indices.clear();
        for (size_t i = 0; i < m_values.size(); ++i) {
            m_time_indices.emplace_back(static_cast<int64_t>(i));
        }
        return *this;
    }

    /**
     * @brief Create a constant value signal
     * @param value Constant value
     * @param start_time Starting time index
     * @param end_time Ending time index (inclusive)
     * @param step Time step between samples
     */
    AnalogTimeSeriesBuilder& withConstant(float value, int start_time, int end_time, int step = 1) {
        m_values.clear();
        m_time_indices.clear();
        
        for (int t = start_time; t <= end_time; t += step) {
            m_values.push_back(value);
            m_time_indices.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Create a triangular wave (0 -> peak -> 0)
     * @param start_time Starting time index
     * @param end_time Ending time index (inclusive)
     * @param peak_value Peak value at midpoint
     */
    AnalogTimeSeriesBuilder& withTriangleWave(int start_time, int end_time, float peak_value) {
        m_values.clear();
        m_time_indices.clear();
        
        int mid = (start_time + end_time) / 2;
        for (int t = start_time; t <= end_time; ++t) {
            float value;
            if (t <= mid) {
                // Rising edge
                value = peak_value * static_cast<float>(t - start_time) / static_cast<float>(mid - start_time);
            } else {
                // Falling edge
                value = peak_value * static_cast<float>(end_time - t) / static_cast<float>(end_time - mid);
            }
            m_values.push_back(value);
            m_time_indices.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Create a sine wave
     * @param start_time Starting time index
     * @param end_time Ending time index (inclusive)
     * @param frequency Frequency in cycles per (end_time - start_time) period
     * @param amplitude Amplitude of the wave
     * @param phase Phase offset in radians
     */
    AnalogTimeSeriesBuilder& withSineWave(int start_time, int end_time, 
                                          float frequency = 0.01f,  // 1 cycle per 100 time units
                                          float amplitude = 100.0f, 
                                          float phase = 0.0f) {
        m_values.clear();
        m_time_indices.clear();
        
        for (int t = start_time; t <= end_time; ++t) {
            float value = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * frequency * static_cast<float>(t) + phase);
            m_values.push_back(value);
            m_time_indices.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Create a cosine wave
     * @param start_time Starting time index
     * @param end_time Ending time index (inclusive)
     * @param frequency Frequency parameter
     * @param amplitude Amplitude of the wave
     * @param phase Phase offset in radians
     */
    AnalogTimeSeriesBuilder& withCosineWave(int start_time, int end_time, 
                                            float frequency = 0.01f, 
                                            float amplitude = 100.0f, 
                                            float phase = 0.0f) {
        m_values.clear();
        m_time_indices.clear();
        
        for (int t = start_time; t <= end_time; ++t) {
            float value = amplitude * std::cos(2.0f * static_cast<float>(M_PI) * frequency * static_cast<float>(t) + phase);
            m_values.push_back(value);
            m_time_indices.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Create a square wave
     * @param start_time Starting time index
     * @param end_time Ending time index (inclusive)
     * @param period Period of the square wave
     * @param high_value High level value
     * @param low_value Low level value
     */
    AnalogTimeSeriesBuilder& withSquareWave(int start_time, int end_time, 
                                            int period, 
                                            float high_value = 1.0f, 
                                            float low_value = 0.0f) {
        m_values.clear();
        m_time_indices.clear();
        
        for (int t = start_time; t <= end_time; ++t) {
            float value = ((t / period) % 2 == 0) ? high_value : low_value;
            m_values.push_back(value);
            m_time_indices.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Create a ramp (linear increase)
     * @param start_time Starting time index
     * @param end_time Ending time index (inclusive)
     * @param start_value Starting value
     * @param end_value Ending value
     */
    AnalogTimeSeriesBuilder& withRamp(int start_time, int end_time, 
                                      float start_value, float end_value) {
        m_values.clear();
        m_time_indices.clear();
        
        int count = end_time - start_time + 1;
        float slope = (end_value - start_value) / static_cast<float>(count - 1);
        
        for (int i = 0; i < count; ++i) {
            m_values.push_back(start_value + slope * static_cast<float>(i));
            m_time_indices.emplace_back(start_time + i);
        }
        return *this;
    }

    /**
     * @brief Create a custom waveform using a function
     * @param start_time Starting time index
     * @param end_time Ending time index (inclusive)
     * @param func Function taking time index and returning value
     */
    AnalogTimeSeriesBuilder& withFunction(int start_time, int end_time, 
                                          std::function<float(int)> func) {
        m_values.clear();
        m_time_indices.clear();
        
        for (int t = start_time; t <= end_time; ++t) {
            m_values.push_back(func(t));
            m_time_indices.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Build the AnalogTimeSeries
     * @return Shared pointer to constructed AnalogTimeSeries
     */
    std::shared_ptr<AnalogTimeSeries> build() const {
        return std::make_shared<AnalogTimeSeries>(m_values, m_time_indices);
    }

    /**
     * @brief Get the values (for inspection)
     */
    const std::vector<float>& getValues() const {
        return m_values;
    }

    /**
     * @brief Get the time indices (for inspection)
     */
    const std::vector<TimeFrameIndex>& getTimeIndices() const {
        return m_time_indices;
    }

    /**
     * @brief Write data to binary file as int16
     * @param filepath Path to write the binary file
     * @param header_size Number of bytes to write as header (filled with zeros)
     * @param num_channels Number of channels to write (if > 1, data is interleaved)
     * @return true if write succeeded, false otherwise
     */
    bool writeToBinaryInt16(std::string const& filepath, size_t header_size = 0, size_t num_channels = 1) const;

    /**
     * @brief Write data to binary file as float32
     * @param filepath Path to write the binary file
     * @param header_size Number of bytes to write as header (filled with zeros)
     * @param num_channels Number of channels to write (if > 1, data is interleaved)
     * @return true if write succeeded, false otherwise
     */
    bool writeToBinaryFloat32(std::string const& filepath, size_t header_size = 0, size_t num_channels = 1) const;

private:
    std::vector<float> m_values;
    std::vector<TimeFrameIndex> m_time_indices;
};

// Implementation of binary write methods
inline bool AnalogTimeSeriesBuilder::writeToBinaryInt16(std::string const& filepath, size_t header_size, size_t num_channels) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Write header (zeros)
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), header_size);
    }

    // Write data
    if (num_channels == 1) {
        // Single channel - write sequentially
        for (float val : m_values) {
            int16_t int_val = static_cast<int16_t>(val);
            file.write(reinterpret_cast<char const*>(&int_val), sizeof(int16_t));
        }
    } else {
        // Multiple channels - repeat the same data for each channel (interleaved)
        // This simulates multiple channels with identical data
        for (float val : m_values) {
            for (size_t ch = 0; ch < num_channels; ++ch) {
                int16_t int_val = static_cast<int16_t>(val + static_cast<float>(ch));
                file.write(reinterpret_cast<char const*>(&int_val), sizeof(int16_t));
            }
        }
    }

    return file.good();
}

inline bool AnalogTimeSeriesBuilder::writeToBinaryFloat32(std::string const& filepath, size_t header_size, size_t num_channels) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Write header (zeros)
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), header_size);
    }

    // Write data
    if (num_channels == 1) {
        // Single channel - write sequentially
        for (float val : m_values) {
            file.write(reinterpret_cast<char const*>(&val), sizeof(float));
        }
    } else {
        // Multiple channels - repeat the same data for each channel (interleaved)
        // This simulates multiple channels with distinct data
        for (float val : m_values) {
            for (size_t ch = 0; ch < num_channels; ++ch) {
                float channel_val = val + static_cast<float>(ch);
                file.write(reinterpret_cast<char const*>(&channel_val), sizeof(float));
            }
        }
    }

    return file.good();
}

#endif // ANALOG_TIME_SERIES_BUILDER_HPP
