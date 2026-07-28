/**
 * @file RowGatherGeometry.cpp
 * @brief Implementation of tensor row-pipeline gather window resolution.
 */

#include "RowGatherGeometry.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include <rfl/json.hpp>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <stdexcept>
#include <string>

namespace Neuralyzer::Gather {

namespace {

[[nodiscard]] bool isWhitespaceOnly(std::string_view text) {
    return std::ranges::all_of(text, [](char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    });
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
        auto const error = descriptor_result.error();
        auto const error_message =
                error ? std::string(error->what()) : std::string("unknown error");
        throw std::runtime_error(
                "isIdentityRowPipelineJson: failed to parse row_pipeline_json: " +
                error_message);
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

    if (!isIdentityRowPipelineJson(row_pipeline_json)) {
        throw std::runtime_error(
                "resolveIntervalGatherWindows: non-identity row_pipeline_json is "
                "not supported until DigitalIntervalSeries row-pipeline dispatch "
                "is implemented");
    }

    if (intervals->size() != expected_row_count) {
        throw std::runtime_error(
                "resolveIntervalGatherWindows: resolved row windows changed row count");
    }

    return intervals;
}

}// namespace Neuralyzer::Gather
