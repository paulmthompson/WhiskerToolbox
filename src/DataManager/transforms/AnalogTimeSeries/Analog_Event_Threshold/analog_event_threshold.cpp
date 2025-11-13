#include "analog_event_threshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <iostream>
#include <vector>// std::vector


std::shared_ptr<DigitalEventSeries> event_threshold(
        AnalogTimeSeries const * analog_time_series,
        ThresholdParams const & thresholdParams) {
    // Call the version with a null progress callback
    return event_threshold(analog_time_series, thresholdParams, nullptr);
}

/**
 * @brief Detects events in an AnalogTimeSeries based on a threshold, with progress reporting.
 *
 * This function identifies time points where the analog signal crosses a specified threshold,
 * considering a lockout period to prevent multiple detections for a single event.
 *
 * @param analog_time_series A pointer to the input AnalogTimeSeries data. Must not be null.
 * @param thresholdParams A struct containing the threshold value, detection direction (positive, negative, or absolute),
 *                        and lockout time (in the same units as the timestamps in analog_time_series).
 * @param progressCallback An optional function that can be called to report progress (0-100).
 *                         If nullptr, progress is not reported.
 * @return A std::shared_ptr<DigitalEventSeries> containing the timestamps of detected events.
 *         Returns an empty DigitalEventSeries if analog_time_series is null, has no data,
 *         or if no events are detected.
 */
std::shared_ptr<DigitalEventSeries> event_threshold(
        AnalogTimeSeries const * analog_time_series,
        ThresholdParams const & thresholdParams,
        ProgressCallback progressCallback) {

    if (!analog_time_series) {
        std::cerr << "Error: analog_time_series is null." << std::endl;
        return std::make_shared<DigitalEventSeries>();
    }

    float const threshold = static_cast<float>(thresholdParams.thresholdValue);
    std::vector<float> events;

    auto const & values = analog_time_series->getAnalogTimeSeries();
    auto const & time_storage = analog_time_series->getTimeStorage();

    if (values.empty()) {
        if (progressCallback) {
            progressCallback(100);// No data to process, so 100% complete.
        }
        return std::make_shared<DigitalEventSeries>();
    }

    double last_ts = -thresholdParams.lockoutTime - 1.0;// Initialize to allow first event
    size_t const total_samples = values.size();

    for (size_t i = 0; i < total_samples; ++i) {
        bool event_detected = false;
        if (thresholdParams.direction == ThresholdParams::ThresholdDirection::POSITIVE) {
            if (values[i] > threshold) {
                event_detected = true;
            }
        } else if (thresholdParams.direction == ThresholdParams::ThresholdDirection::NEGATIVE) {
            if (values[i] < threshold) {
                event_detected = true;
            }
        } else if (thresholdParams.direction == ThresholdParams::ThresholdDirection::ABSOLUTE) {
            if (std::abs(values[i]) > threshold) {
                event_detected = true;
            }
        } else {
            std::cerr << "Unknown threshold direction!" << std::endl;
            if (progressCallback) {
                progressCallback(100);// Error case, consider it done.
            }
            return std::make_shared<DigitalEventSeries>();// Return empty series on error
        }

        if (event_detected) {

            auto timestamp = std::visit([i](auto const & storage) -> TimeFrameIndex {
                return storage.getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i));
            }, time_storage);

            // Check if the event is not too close to the last one
            if (static_cast<double>(timestamp.getValue()) - last_ts >= thresholdParams.lockoutTime) {
                events.push_back(static_cast<float>(timestamp.getValue()));
                last_ts = static_cast<double>(timestamp.getValue());
            }
        }

        if (progressCallback && total_samples > 0) {
            int const current_progress = static_cast<int>((static_cast<double>(i + 1) / static_cast<double>(total_samples)) * 100.0);
            progressCallback(current_progress);
        }
    }

    auto event_series = std::make_shared<DigitalEventSeries>(events);
    if (progressCallback) {
        progressCallback(100);// Ensure 100% is reported at the end.
    }
    return event_series;
}

///////////////////////////////////////////////////////////////////////////////

std::string EventThresholdOperation::getName() const {
    return "Threshold Event Detection";
}

std::type_index EventThresholdOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

bool EventThresholdOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<AnalogTimeSeries>(dataVariant);
}

std::unique_ptr<TransformParametersBase> EventThresholdOperation::getDefaultParameters() const {
    return std::make_unique<ThresholdParams>();
}

DataTypeVariant EventThresholdOperation::execute(DataTypeVariant const & dataVariant, TransformParametersBase const * transformParameters) {
    // Call the version with a null progress callback
    return execute(dataVariant, transformParameters, nullptr);
}

/**
 * @brief Executes the event thresholding operation with progress reporting.
 *
 * This method retrieves an AnalogTimeSeries from the input dataVariant,
 * applies the event thresholding logic using the provided parameters,
 * and reports progress via the progressCallback.
 *
 * @param dataVariant A variant holding a std::shared_ptr<AnalogTimeSeries>.
 *                    The operation will fail if the variant holds a different type,
 *                    or if the shared_ptr is null.
 * @param transformParameters A pointer to TransformParametersBase, which is expected
 *                            to be dynamically castable to ThresholdParams. If null or
 *                            of an incorrect type, default ThresholdParams will be used
 *                            (if appropriate for the operation) or an error may occur.
 * @param progressCallback An optional function to report progress (0-100).
 *                         If nullptr, progress is not reported.
 * @return A DataTypeVariant containing a std::shared_ptr<DigitalEventSeries> with the
 *         detected event times on success. Returns an empty DataTypeVariant on failure
 *         (e.g., type mismatch, null input data, or if the underlying
 *         event_threshold function fails).
 */
DataTypeVariant EventThresholdOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters,
                                                 ProgressCallback progressCallback) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "EventThresholdOperation::execute called with incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);// Indicate completion even on error
        return {};                                  // Return empty
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    ThresholdParams currentParams;// Default parameters

    if (transformParameters != nullptr) {
        auto const * specificParams = dynamic_cast<ThresholdParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
            // std::cout << "Using parameters provided by UI." << std::endl; // Debug, consider removing
        } else {
            std::cerr << "Warning: EventThresholdOperation received incompatible parameter type (dynamic_cast failed)! Using default parameters." << std::endl;
            // Fall through to use the default 'currentParams'
        }
    } else {
        // std::cout << "ThresholdOperation received null parameters. Using default parameters." << std::endl; // Debug, consider removing
        // Fall through to use the default 'currentParams'
    }

    std::shared_ptr<DigitalEventSeries> result_ts = event_threshold(analog_raw_ptr,
                                                                    currentParams,
                                                                    progressCallback);

    if (!result_ts) {
        std::cerr << "EventThresholdOperation::execute: 'event_threshold' failed to produce a result." << std::endl;
        if (progressCallback) progressCallback(100);// Indicate completion even on error
        return {};                                  // Return empty
    }

    // std::cout << "EventThresholdOperation executed successfully using variant input." << std::endl; // Debug, consider removing
    if (progressCallback) progressCallback(100);// Ensure 100% is reported at the end.
    return result_ts;
}
