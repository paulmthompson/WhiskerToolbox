#include "analog_event_threshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"


std::shared_ptr<DigitalEventSeries> event_threshold(
        AnalogTimeSeries const * analog_time_series,
        ThresholdParams const & thresholdParams) {
    auto event_series = std::make_shared<DigitalEventSeries>();

    float const threshold = thresholdParams.thresholdValue;

    std::vector<float> events;

    auto const & timestamps = analog_time_series->getTimeSeries();
    auto const & values = analog_time_series->getAnalogTimeSeries();

    double last_ts = 0.0;
    if (thresholdParams.direction == ThresholdParams::ThresholdDirection::POSITIVE) {
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (values[i] > threshold) {
                // Check if the event is not too close to the last one
                if (timestamps[i] - last_ts < thresholdParams.lockoutTime) {
                    continue;
                }
                events.push_back(timestamps[i]);
                last_ts = timestamps[i];
            }
        }
    } else if (thresholdParams.direction == ThresholdParams::ThresholdDirection::NEGATIVE) {
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (values[i] < threshold) {
                // Check if the event is not too close to the last one
                if (timestamps[i] - last_ts < thresholdParams.lockoutTime) {
                    continue;
                }
                events.push_back(timestamps[i]);
                last_ts = timestamps[i];
            }
        }
    } else if (thresholdParams.direction == ThresholdParams::ThresholdDirection::ABSOLUTE) {
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (std::abs(values[i]) > threshold) {
                // Check if the event is not too close to the last one
                if (timestamps[i] - last_ts < thresholdParams.lockoutTime) {
                    continue;
                }
                events.push_back(timestamps[i]);
                last_ts = timestamps[i];
            }
        }
    } else {
        std::cerr << "Unknown threshold direction!" << std::endl;
    }

    event_series->setData(events);

    return event_series;
}

std::string EventThresholdOperation::getName() const {
    return "Threshold Event Detection";
}

std::type_index EventThresholdOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

bool EventThresholdOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    // Return true only if get_if succeeded AND the contained shared_ptr is not null.
    return ptr_ptr && *ptr_ptr;
}

DataTypeVariant EventThresholdOperation::execute(DataTypeVariant const & dataVariant, TransformParametersBase const * transformParameters) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "EventThresholdOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    ThresholdParams currentParams;

    if (transformParameters != nullptr) {
        ThresholdParams const * specificParams = dynamic_cast<ThresholdParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
            std::cout << "Using parameters provided by UI." << std::endl;
        } else {
            std::cerr << "Warning: ThresholdOperation received incompatible parameter type (dynamic_cast failed)! Using default parameters." << std::endl;
        }
    } else {
        // No parameter object was provided (nullptr). This might be expected if the
        // operation can run with defaults or was called programmatically.
        std::cout << "ThresholdOperation received null parameters. Using default parameters." << std::endl;
        // Fall through to use the default 'currentParams'
    }

    std::shared_ptr<DigitalEventSeries> result_ts = event_threshold(analog_raw_ptr,
                                                                    currentParams);

    if (!result_ts) {
        std::cerr << "EventThresholdOperation::execute: 'calculate_events' failed to produce a result." << std::endl;
        return {};// Return empty
    }

    std::cout << "EventThresholdOperation executed successfully using variant input." << std::endl;
    return result_ts;
}
