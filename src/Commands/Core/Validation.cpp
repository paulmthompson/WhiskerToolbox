/**
 * @file Validation.cpp
 * @brief Implementation of pre-execution command sequence validation
 */

#include "Validation.hpp"

#include "AddInterval.hpp"
#include "CommandContext.hpp"
#include "CommandDescriptor.hpp"
#include "CommandFactory.hpp"
#include "CopyByTimeRange.hpp"
#include "ForEachKey.hpp"
#include "MoveByTimeRange.hpp"
#include "VariableSubstitution.hpp"

#include "DataManager/DataManager.hpp"

#include <rfl/json.hpp>

namespace commands {

std::vector<std::string> findUnresolvedVariables(std::string const & input) {
    std::vector<std::string> unresolved;
    std::string::size_type pos = 0;

    while ((pos = input.find("${", pos)) != std::string::npos) {
        auto const end_pos = input.find('}', pos + 2);
        if (end_pos == std::string::npos) {
            break;
        }
        unresolved.push_back(input.substr(pos + 2, end_pos - pos - 2));
        pos = end_pos + 1;
    }

    return unresolved;
}

namespace {

/// Validate data keys and type compatibility for MoveByTimeRange / CopyByTimeRange
void validateMoveOrCopyKeys(
        std::string const & source_key,
        std::string const & destination_key,
        std::string const & prefix,
        DataManager & dm,
        ValidationResult & result) {

    auto src = dm.getDataVariant(source_key);
    auto dst = dm.getDataVariant(destination_key);

    // Check 4: Data key existence
    if (!src) {
        result.warnings.push_back(
                prefix + ": source key '" + source_key +
                "' not found in DataManager");
    }
    if (!dst) {
        result.warnings.push_back(
                prefix + ": destination key '" + destination_key +
                "' not found in DataManager");
    }

    // Check 5: Type compatibility
    if (src && dst && src->index() != dst->index()) {
        result.errors.push_back(
                prefix + ": source and destination have incompatible types");
        result.valid = false;
    }
}

}// namespace

ValidationResult validateSequence(
        CommandSequenceDescriptor const & seq,
        CommandContext const & ctx) {

    ValidationResult result;

    for (size_t i = 0; i < seq.commands.size(); ++i) {
        auto const & desc = seq.commands[i];
        auto const index_str = std::to_string(i);

        // Apply variable substitution to command name
        auto resolved_name = desc.command_name;
        if (seq.variables) {
            resolved_name =
                    substituteVariables(resolved_name, *seq.variables);
        }
        if (!ctx.runtime_variables.empty()) {
            resolved_name =
                    substituteVariables(resolved_name, ctx.runtime_variables);
        }

        std::string prefix = "Command ";
        prefix += index_str;
        prefix += " ('";
        prefix += resolved_name;
        prefix += "')";

        // Check 1: Structural — command name is recognized by the factory
        if (!isKnownCommandName(resolved_name)) {
            result.errors.push_back(
                    prefix + ": unknown command name");
            result.valid = false;
            continue;
        }

        // Apply variable substitution to parameters
        rfl::Generic resolved_params;
        std::string resolved_json;
        bool has_unresolved = false;

        if (desc.parameters) {
            resolved_json = rfl::json::write(*desc.parameters);
            if (seq.variables) {
                resolved_json =
                        substituteVariables(resolved_json, *seq.variables);
            }
            if (!ctx.runtime_variables.empty()) {
                resolved_json =
                        substituteVariables(resolved_json, ctx.runtime_variables);
            }

            // Check 3: Variable resolution
            // For ForEachKey, the iteration variable is defined within its own
            // params — extract it before checking for unresolved references.
            std::string foreach_variable;
            if (resolved_name == "ForEachKey") {
                auto fe =
                        rfl::json::read<ForEachKeyParams>(resolved_json);
                if (fe) {
                    foreach_variable = fe.value().variable;
                }
            }

            auto const unresolved = findUnresolvedVariables(resolved_json);
            for (auto const & var: unresolved) {
                if (var == foreach_variable) {
                    continue;// ForEachKey's own iteration variable
                }
                has_unresolved = true;
                std::string msg = prefix;
                msg += ": unresolved variable '${";
                msg += var;
                msg += "}'";
                result.errors.push_back(std::move(msg));
                result.valid = false;
            }

            auto parsed = rfl::json::read<rfl::Generic>(resolved_json);
            if (!parsed) {
                result.errors.push_back(
                        prefix +
                        ": failed to parse substituted parameters as JSON");
                result.valid = false;
                continue;
            }
            resolved_params = std::move(*parsed);
        }

        // Check 2: Deserialization — skip if unresolved variables would cause
        //           false negatives (the ${...} string won't match expected types)
        if (has_unresolved) {
            continue;
        }

        auto cmd = createCommand(resolved_name, resolved_params);
        if (!cmd) {
            result.errors.push_back(
                    prefix +
                    ": parameters failed to deserialize into expected struct");
            result.valid = false;
            continue;
        }

        // Checks 4 & 5: Data key existence and type compatibility
        if (ctx.data_manager) {
            if (resolved_name == "MoveByTimeRange") {
                auto p = rfl::json::read<MoveByTimeRangeParams>(resolved_json);
                if (p) {
                    auto const & params = p.value();
                    validateMoveOrCopyKeys(
                            params.source_key, params.destination_key,
                            prefix, *ctx.data_manager, result);
                }
            } else if (resolved_name == "CopyByTimeRange") {
                auto p = rfl::json::read<CopyByTimeRangeParams>(resolved_json);
                if (p) {
                    auto const & params = p.value();
                    validateMoveOrCopyKeys(
                            params.source_key, params.destination_key,
                            prefix, *ctx.data_manager, result);
                }
            } else if (resolved_name == "AddInterval") {
                auto p = rfl::json::read<AddIntervalParams>(resolved_json);
                if (p) {
                    auto const & params = p.value();
                    if (!params.create_if_missing) {
                        auto data = ctx.data_manager->getDataVariant(
                                params.interval_key);
                        if (!data) {
                            result.warnings.push_back(
                                    prefix + ": interval key '" +
                                    params.interval_key +
                                    "' not found and create_if_missing is false");
                        }
                    }
                }
            }
        }

        // ForEachKey: recursively validate sub-commands
        if (resolved_name == "ForEachKey") {
            auto p = rfl::json::read<ForEachKeyParams>(resolved_json);
            if (p) {
                auto const & params = p.value();
                CommandSequenceDescriptor sub_seq;
                sub_seq.commands = params.commands;
                sub_seq.variables = seq.variables;

                // Create a context without DataManager so checks 4–5 are
                // skipped for templated sub-commands, but add the iteration
                // variable as resolved so check 3 passes.
                CommandContext sub_ctx;
                sub_ctx.runtime_variables = ctx.runtime_variables;
                sub_ctx.time_controller = ctx.time_controller;
                sub_ctx.runtime_variables[params.variable] = "__foreach_placeholder__";

                auto sub_result = validateSequence(sub_seq, sub_ctx);
                for (auto const & err: sub_result.errors) {
                    std::string msg = prefix;
                    msg += " → ";
                    msg += err;
                    result.errors.push_back(std::move(msg));
                }
                for (auto const & warn: sub_result.warnings) {
                    std::string msg = prefix;
                    msg += " → ";
                    msg += warn;
                    result.warnings.push_back(std::move(msg));
                }
                if (!sub_result.valid) {
                    result.valid = false;
                }
            }
        }
    }

    return result;
}

}// namespace commands
