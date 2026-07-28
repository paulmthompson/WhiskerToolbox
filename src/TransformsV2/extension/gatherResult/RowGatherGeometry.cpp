/**
 * @file RowGatherGeometry.cpp
 * @brief Implementation of tensor row-pipeline gather window resolution.
 */

#include "RowGatherGeometry.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include <rfl/json.hpp>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace Neuralyzer::Gather {

namespace {

/**
 * @brief Check whether text contains only whitespace characters.
 * @param text Text to inspect.
 * @return True when every character is whitespace.
 * @post Does not allocate or modify the input string.
 */
[[nodiscard]] bool isWhitespaceOnly(std::string_view text) {
    return std::ranges::all_of(text, [](char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    });
}

/**
 * @brief Convert a reflect-cpp error result to a stable message string.
 * @param error Optional reflect-cpp error object.
 * @return Human-readable error text.
 * @post Returns "unknown error" when no error object is available.
 */
[[nodiscard]] std::string errorMessage(auto const & error) {
    return error ? std::string(error->what()) : std::string("unknown error");
}

/**
 * @brief Load row-pipeline JSON into an executable TransformPipeline.
 * @param row_pipeline_json Embedded PipelineDescriptor JSON string.
 * @return Loaded TransformPipeline.
 * @post The returned pipeline is non-empty if loading succeeds.
 *
 * @throws std::runtime_error if the JSON cannot be loaded as a TransformPipeline.
 */
[[nodiscard]] Neuralyzer::Transforms::V2::TransformPipeline loadRowPipeline(
        std::string_view row_pipeline_json) {
    auto pipeline_result =
            Neuralyzer::Transforms::V2::Examples::loadPipelineFromJson(
                    std::string{row_pipeline_json});
    if (!pipeline_result) {
        throw std::runtime_error(
                "resolveIntervalGatherWindows: failed to load row_pipeline_json: " +
                errorMessage(pipeline_result.error()));
    }

    return std::move(pipeline_result.value());
}

/**
 * @brief Extract a DigitalIntervalSeries pointer from a row-pipeline output variant.
 * @param output Row-pipeline DataTypeVariant output.
 * @return DigitalIntervalSeries output as prepared gather windows.
 * @post The returned pointer aliases the pipeline output object.
 *
 * @throws std::runtime_error if the output is DigitalEventSeries or another type.
 */
[[nodiscard]] std::shared_ptr<DigitalIntervalSeries const> requireIntervalOutput(
        DataTypeVariant const & output) {
    if (auto const * intervals = std::get_if<std::shared_ptr<DigitalIntervalSeries>>(&output);
        intervals != nullptr && *intervals) {
        return *intervals;
    }

    if (std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(output)) {
        throw std::runtime_error(
                "resolveIntervalGatherWindows: row_pipeline_json produced "
                "DigitalEventSeries sample times, which are deferred until "
                "timestamp/sample row-pipeline dispatch is implemented");
    }

    throw std::runtime_error(
            "resolveIntervalGatherWindows: row_pipeline_json must produce "
            "DigitalIntervalSeries prepared windows for interval-row columns");
}

/**
 * @brief Verify that resolved gather windows preserve tensor row count.
 * @pre windows must not be null.
 * @param windows Resolved gather windows.
 * @param expected_row_count Number of rows from the original tensor row source.
 * @post The function returns only when the row count is unchanged.
 *
 * @throws std::runtime_error if windows is null or has a different size.
 */
void verifyResolvedRowCount(
        std::shared_ptr<DigitalIntervalSeries const> const & windows,
        std::size_t expected_row_count) {
    assert(windows && "verifyResolvedRowCount: windows must not be null");
    if (!windows) {
        throw std::runtime_error(
                "resolveIntervalGatherWindows: resolved row windows must not be null");
    }

    if (windows->size() != expected_row_count) {
        throw std::runtime_error(
                "resolveIntervalGatherWindows: resolved row windows changed row count");
    }
}

}// namespace

bool isIdentityRowPipelineJson(std::string_view row_pipeline_json) {
    if (row_pipeline_json.empty() || isWhitespaceOnly(row_pipeline_json)) {
        return true;
    }

    auto descriptor_result =
            rfl::json::read<Neuralyzer::Transforms::V2::Examples::PipelineDescriptor>(
                    std::string{row_pipeline_json});
    if (!descriptor_result) {
        throw std::runtime_error(
                "isIdentityRowPipelineJson: failed to parse row_pipeline_json: " +
                errorMessage(descriptor_result.error()));
    }

    auto const & descriptor = descriptor_result.value();
    bool const has_pre_reductions =
            descriptor.pre_reductions.has_value() &&
            !descriptor.pre_reductions.value().empty();
    return descriptor.steps.empty() && !descriptor.range_reduction.has_value() &&
           !has_pre_reductions;
}

std::shared_ptr<DigitalIntervalSeries const> resolveIntervalGatherWindows(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        std::string_view row_pipeline_json,
        std::size_t expected_row_count) {
    assert(intervals && "resolveIntervalGatherWindows: intervals must not be null");
    if (!intervals) {
        throw std::runtime_error(
                "resolveIntervalGatherWindows: intervals must not be null");
    }

    if (isIdentityRowPipelineJson(row_pipeline_json)) {
        verifyResolvedRowCount(intervals, expected_row_count);
        return intervals;
    }

    auto row_pipeline = loadRowPipeline(row_pipeline_json);
    // DataTypeVariant currently stores mutable shared_ptr alternatives, but
    // executePipeline reads its input through const references.
    DataTypeVariant const row_input =
            std::const_pointer_cast<DigitalIntervalSeries>(intervals);
    auto const row_output =
            Neuralyzer::Transforms::V2::executePipeline(row_input, row_pipeline);
    auto resolved_windows = requireIntervalOutput(row_output);

    verifyResolvedRowCount(resolved_windows, expected_row_count);
    return resolved_windows;
}

}// namespace Neuralyzer::Gather
