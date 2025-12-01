#include "AnalogEventThreshold.hpp"

#include <cmath>
#include <iostream>

namespace WhiskerToolbox::Transforms::V2::Examples {

std::shared_ptr<DigitalEventSeries> analogEventThreshold(
    AnalogTimeSeries const& input,
    AnalogEventThresholdParams const& params,
    ComputeContext const& ctx) {
    
    // Validate parameters
    if (!params.isValidDirection()) {
        std::cerr << "Invalid direction parameter: " << params.getDirection() << std::endl;
        return std::make_shared<DigitalEventSeries>();
    }
    
    float const threshold = params.getThresholdValue();
    float const lockout_time = params.getLockoutTime();
    std::string const direction = params.getDirection();
    
    auto const& values = input.getAnalogTimeSeries();
    auto const& time_storage = input.getTimeStorage();
    
    if (values.empty()) {
        ctx.reportProgress(100);
        return std::make_shared<DigitalEventSeries>();
    }
    
    std::vector<TimeFrameIndex> events;
    double last_event_time = -lockout_time - 1.0;  // Initialize to allow first event
    size_t const total_samples = values.size();
    
    // Report initial progress
    ctx.reportProgress(0);
    
    for (size_t i = 0; i < total_samples; ++i) {
        // Check for cancellation periodically (every 100 samples or so)
        if (i % 100 == 0 && ctx.shouldCancel()) {
            // Early exit due to cancellation
            return std::make_shared<DigitalEventSeries>();
        }
        
        bool event_detected = false;
        
        // Check threshold crossing based on direction
        if (direction == "positive") {
            if (values[i] > threshold) {
                event_detected = true;
            }
        } else if (direction == "negative") {
            if (values[i] < threshold) {
                event_detected = true;
            }
        } else if (direction == "absolute") {
            if (std::abs(values[i]) > std::abs(threshold)) {
                event_detected = true;
            }
        }
        
        if (event_detected) {
            auto timestamp = time_storage->getTimeFrameIndexAt(i);
            double current_time = static_cast<double>(timestamp.getValue());
            
            // Check if event is outside lockout period
            if (current_time - last_event_time >= lockout_time) {
                events.push_back(timestamp);
                last_event_time = current_time;
            }
        }
        
        // Report progress periodically
        if (i % 100 == 0 || i == total_samples - 1) {
            int const progress = static_cast<int>(
                (static_cast<double>(i + 1) / static_cast<double>(total_samples)) * 100.0);
            ctx.reportProgress(progress);
        }
    }
    
    // Ensure 100% is reported at the end
    ctx.reportProgress(100);
    
    return std::make_shared<DigitalEventSeries>(events);
}

} // namespace WhiskerToolbox::Transforms::V2::Examples
