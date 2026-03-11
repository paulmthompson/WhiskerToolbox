/**
 * @file Validation.hpp
 * @brief Pre-execution validation of command sequences
 */

#ifndef COMMAND_VALIDATION_HPP
#define COMMAND_VALIDATION_HPP

#include <string>
#include <vector>

namespace commands {

struct CommandContext;
struct CommandSequenceDescriptor;

/// @brief Result of validating a command sequence before execution
struct ValidationResult {
    bool valid = true;

    /// Non-fatal issues (e.g., data keys not yet present in DataManager)
    std::vector<std::string> warnings;

    /// Fatal issues that would cause execution failure
    std::vector<std::string> errors;
};

/// @brief Validate a command sequence without executing it.
///
/// Performs five checks in order of increasing specificity:
///   1. Structural: all command_name values map to known commands in the factory
///   2. Deserialization: parameters deserialize into expected param structs
///   3. Variable resolution: all ${variable} references resolve
///   4. Data key existence (optional, requires DataManager): referenced keys exist
///   5. Type compatibility (optional, requires DataManager): move/copy source and
///      destination resolve to the same DataTypeVariant alternative
///
/// Checks 1–3 run without a DataManager. Checks 4–5 only run when
/// ctx.data_manager is non-null.
///
/// @param seq The command sequence descriptor to validate
/// @param ctx Runtime context (for variable resolution and optional DataManager)
/// @return ValidationResult with any errors and warnings found
ValidationResult validateSequence(
        CommandSequenceDescriptor const & seq,
        CommandContext const & ctx);

/// @brief Find all unresolved ${variable} references in a string.
///
/// Returns the variable names (without the ${} delimiters) for each reference
/// that remains in the string.
std::vector<std::string> findUnresolvedVariables(std::string const & input);

}// namespace commands

#endif// COMMAND_VALIDATION_HPP
