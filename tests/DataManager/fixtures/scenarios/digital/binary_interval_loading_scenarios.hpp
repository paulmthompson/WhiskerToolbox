#ifndef DIGITAL_INTERVAL_BINARY_LOADING_SCENARIOS_HPP
#define DIGITAL_INTERVAL_BINARY_LOADING_SCENARIOS_HPP

#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace digital_interval_binary_scenarios {

/**
 * @brief Write binary file containing packed TTL data for a single channel
 * 
 * Creates a uint16_t binary file where the specified bit (channel) is set
 * to 1 during intervals and 0 otherwise. This simulates TTL pulse data
 * where rising edges start intervals and falling edges end them.
 * 
 * @param intervals The DigitalIntervalSeries containing intervals to encode
 * @param filepath Output file path
 * @param total_samples Total number of samples in the file
 * @param channel Bit position (0-15) to encode intervals in
 * @param header_size Number of header bytes to write (filled with zeros)
 * @return true if write succeeded
 */
inline bool writeBinaryUint16(DigitalIntervalSeries const* intervals,
                              std::string const& filepath,
                              size_t total_samples,
                              int channel = 0,
                              size_t header_size = 0) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header (zeros)
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), static_cast<std::streamsize>(header_size));
    }
    
    // Create sample buffer
    std::vector<uint16_t> samples(total_samples, 0);
    
    // Set bits during intervals
    uint16_t const mask = static_cast<uint16_t>(1u << channel);
    for (auto const& interval : intervals->view()) {
        auto start = static_cast<size_t>(interval.value().start);
        auto end = static_cast<size_t>(interval.value().end);
        
        // Clamp to valid range
        if (start >= total_samples) continue;
        if (end > total_samples) end = total_samples;
        
        for (size_t i = start; i < end; ++i) {
            samples[i] |= mask;
        }
    }
    
    // Write samples
    file.write(reinterpret_cast<char const*>(samples.data()), 
               static_cast<std::streamsize>(samples.size() * sizeof(uint16_t)));
    
    return file.good();
}

/**
 * @brief Write binary file with multiple channels containing different intervals
 * 
 * Each channel can have independent interval patterns encoded in different bits.
 * 
 * @param channel_intervals Map of channel number to intervals for that channel
 * @param filepath Output file path
 * @param total_samples Total number of samples
 * @param header_size Number of header bytes
 * @return true if write succeeded
 */
inline bool writeBinaryUint16MultiChannel(
        std::vector<std::pair<int, std::shared_ptr<DigitalIntervalSeries>>> const& channel_intervals,
        std::string const& filepath,
        size_t total_samples,
        size_t header_size = 0) {
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header (zeros)
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), static_cast<std::streamsize>(header_size));
    }
    
    // Create sample buffer
    std::vector<uint16_t> samples(total_samples, 0);
    
    // Set bits for each channel's intervals
    for (auto const& [channel, intervals] : channel_intervals) {
        uint16_t const mask = static_cast<uint16_t>(1u << channel);
        
        for (auto const& interval : intervals->view()) {
            auto start = static_cast<size_t>(interval.value().start);
            auto end = static_cast<size_t>(interval.value().end);
            
            if (start >= total_samples) continue;
            if (end > total_samples) end = total_samples;
            
            for (size_t i = start; i < end; ++i) {
                samples[i] |= mask;
            }
        }
    }
    
    // Write samples
    file.write(reinterpret_cast<char const*>(samples.data()), 
               static_cast<std::streamsize>(samples.size() * sizeof(uint16_t)));
    
    return file.good();
}

/**
 * @brief Write binary file with uint8_t data type (8 channels max)
 */
inline bool writeBinaryUint8(DigitalIntervalSeries const* intervals,
                             std::string const& filepath,
                             size_t total_samples,
                             int channel = 0,
                             size_t header_size = 0) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    if (header_size > 0) {
        std::vector<char> header(header_size, 0);
        file.write(header.data(), static_cast<std::streamsize>(header_size));
    }
    
    std::vector<uint8_t> samples(total_samples, 0);
    
    uint8_t const mask = static_cast<uint8_t>(1u << channel);
    for (auto const& interval : intervals->view()) {
        auto start = static_cast<size_t>(interval.value().start);
        auto end = static_cast<size_t>(interval.value().end);
        
        if (start >= total_samples) continue;
        if (end > total_samples) end = total_samples;
        
        for (size_t i = start; i < end; ++i) {
            samples[i] |= mask;
        }
    }
    
    file.write(reinterpret_cast<char const*>(samples.data()), 
               static_cast<std::streamsize>(samples.size() * sizeof(uint8_t)));
    
    return file.good();
}

//=============================================================================
// Pre-configured test interval patterns for binary loading tests
//=============================================================================

/**
 * @brief Simple intervals for basic TTL loading tests
 * 
 * Creates intervals at: [10,20], [50,60], [100,120]
 * Total samples needed: at least 130
 */
inline std::shared_ptr<DigitalIntervalSeries> simple_ttl_pulses() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(10, 20)
        .withInterval(50, 60)
        .withInterval(100, 120)
        .build();
}

/**
 * @brief Single long pulse for minimal test
 * 
 * Creates interval at: [25, 75]
 */
inline std::shared_ptr<DigitalIntervalSeries> single_pulse() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(25, 75)
        .build();
}

/**
 * @brief Regular periodic pulses (like a timing signal)
 * 
 * Creates 10 pulses: [5,10], [25,30], [45,50], ..., [185,190]
 * Duration 5, period 20
 * Note: Starts at 5 (not 0) because edge detection requires a preceding sample
 */
inline std::shared_ptr<DigitalIntervalSeries> periodic_pulses() {
    return DigitalIntervalSeriesBuilder()
        .withPattern(5, 200, 5, 15)  // Start at 5, 5 duration, 15 gap (20 period)
        .build();
}

/**
 * @brief Adjacent pulses (touching but not overlapping)
 * 
 * Creates intervals: [5,15], [15,25], [25,35], [35,45]
 * Note: Starts at 5 (not 0) because edge detection requires a preceding sample
 */
inline std::shared_ptr<DigitalIntervalSeries> adjacent_pulses() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(5, 15)
        .withInterval(15, 25)
        .withInterval(25, 35)
        .withInterval(35, 45)
        .build();
}

/**
 * @brief Wide-spaced long pulses
 * 
 * Creates intervals with long gaps: [100,200], [500,600], [1000,1200]
 */
inline std::shared_ptr<DigitalIntervalSeries> wide_spaced_pulses() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(100, 200)
        .withInterval(500, 600)
        .withInterval(1000, 1200)
        .build();
}

/**
 * @brief Minimal duration pulses (1 sample each)
 * 
 * Creates very short pulses: [10,11], [20,21], [30,31]
 */
inline std::shared_ptr<DigitalIntervalSeries> minimal_pulses() {
    return DigitalIntervalSeriesBuilder()
        .withInterval(10, 11)
        .withInterval(20, 21)
        .withInterval(30, 31)
        .withInterval(40, 41)
        .build();
}

/**
 * @brief No pulses (empty intervals, always low)
 */
inline std::shared_ptr<DigitalIntervalSeries> no_pulses() {
    return std::make_shared<DigitalIntervalSeries>();
}

} // namespace digital_interval_binary_scenarios

#endif // DIGITAL_INTERVAL_BINARY_LOADING_SCENARIOS_HPP
