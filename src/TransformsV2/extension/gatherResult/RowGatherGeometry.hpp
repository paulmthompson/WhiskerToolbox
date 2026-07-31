/**
 * @file RowGatherGeometry.hpp
 * @brief Helpers for resolving tensor row-pipeline gather windows.
 */

#ifndef TRANSFORMS_V2_EXTENSION_GATHER_RESULT_ROW_GATHER_GEOMETRY_HPP
#define TRANSFORMS_V2_EXTENSION_GATHER_RESULT_ROW_GATHER_GEOMETRY_HPP

#include <cstddef>
#include <memory>
#include <string_view>
#include <variant>

class DigitalEventSeries;
class DigitalIntervalSeries;

namespace Neuralyzer::Gather {

using RowPipelineGeometry = std::variant<
        std::shared_ptr<DigitalIntervalSeries const>,
        std::shared_ptr<DigitalEventSeries const>>;

/**
 * @brief Check whether row-pipeline JSON represents identity row geometry.
 * @param row_pipeline_json Embedded row-pipeline JSON string.
 * @return True when the JSON is empty/whitespace or contains no executable row steps.
 * @post Does not execute any transform pipeline.
 *
 * @throws std::runtime_error if non-empty JSON cannot be parsed as a pipeline descriptor.
 */
[[nodiscard]] bool isIdentityRowPipelineJson(std::string_view row_pipeline_json);

/**
 * @brief Resolve interval-row gather windows for a tensor column.
 * @pre intervals must not be null.
 * @param intervals Original tensor row-source intervals.
 * @param row_pipeline_json Embedded row-pipeline JSON string.
 * @param expected_row_count Number of rows in the original row source.
 * @return Prepared gather windows for the column.
 * @post Identity row pipelines return the original interval series; non-identity
 *       pipelines return a row-aligned DigitalIntervalSeries result.
 *
 * @throws std::runtime_error if intervals is null, row count changes, JSON is invalid,
 *         or the row pipeline output is not a DigitalIntervalSeries.
 */
[[nodiscard]] std::shared_ptr<DigitalIntervalSeries const> resolveIntervalGatherWindows(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        std::string_view row_pipeline_json,
        std::size_t expected_row_count);

/**
 * @brief Resolve interval-row pipeline output as gather windows or sample times.
 *
 * Identity row pipelines return the original DigitalIntervalSeries windows.
 * Non-identity row pipelines may return DigitalIntervalSeries windows or
 * DigitalEventSeries sample times, both preserving the expected row count.
 */
[[nodiscard]] RowPipelineGeometry resolveIntervalRowPipelineGeometry(
        std::shared_ptr<DigitalIntervalSeries const> intervals,
        std::string_view row_pipeline_json,
        std::size_t expected_row_count);

}// namespace Neuralyzer::Gather

#endif// TRANSFORMS_V2_EXTENSION_GATHER_RESULT_ROW_GATHER_GEOMETRY_HPP
