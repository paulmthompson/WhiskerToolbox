/**
 * @file TransformsV2PipelineOutput.hpp
 * @brief Shared helpers for storing TransformsV2 pipeline command output in DataManager
 */

#ifndef TRANSFORMS_V2_PIPELINE_OUTPUT_HPP
#define TRANSFORMS_V2_PIPELINE_OUTPUT_HPP

#include "DataManager/DataManagerTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <optional>
#include <string>
#include <string_view>

class DataManager;

namespace commands {

/**
 * @brief Store pipeline output into @p output_key, merging when supported.
 *
 * Used by single-frame commands that accumulate results across repeated runs.
 *
 * @pre @p dm is non-null
 * @param command_prefix Prefix for error messages (e.g. command name)
 * @post On success, @p output_key exists with updated or merged data
 */
[[nodiscard]] std::optional<std::string>
storePipelineOutputMerged(DataManager & dm,
                          std::string const & input_key,
                          std::string const & output_key,
                          DataTypeVariant const & output,
                          std::string_view command_prefix);

/**
 * @brief Store pipeline output into @p output_key, replacing any existing object.
 *
 * Used by full-dataset commands that mirror TransformsV2 widget semantics.
 *
 * @pre @p dm is non-null
 * @param command_prefix Prefix for error messages (e.g. command name)
 * @post On success, @p output_key exists with replaced data bound to @p time_key
 */
[[nodiscard]] std::optional<std::string>
storePipelineOutputReplace(DataManager & dm,
                           std::string const & output_key,
                           DataTypeVariant const & output,
                           TimeKey const & time_key,
                           std::string_view command_prefix);

/**
 * @brief Resolve the TimeKey to use when storing pipeline output.
 *
 * @pre @p dm is non-null
 * @param input_key DataManager key used as input to the pipeline
 * @param output_time_key When non-empty, must name an existing TimeFrame in @p dm
 * @param command_prefix Prefix for error messages (e.g. command name)
 * @param error_message Set on failure
 * @return Resolved TimeKey on success, or empty on failure
 */
[[nodiscard]] std::optional<TimeKey>
resolvePipelineOutputTimeKey(DataManager & dm,
                             std::string const & input_key,
                             std::string const & output_time_key,
                             std::string_view command_prefix,
                             std::string & error_message);

}// namespace commands

#endif// TRANSFORMS_V2_PIPELINE_OUTPUT_HPP
