/**
 * @file MutationGuard.hpp
 * @brief Debug-only strangulation pattern detecting DataManager mutations outside commands
 *
 * Provides a thread_local flag and RAII guard to track whether a DataManager mutation
 * occurs inside an ICommand::execute() call. In debug builds, DataManager mutating methods
 * check this flag and emit a warning when mutations happen outside commands.
 */

#ifndef MUTATION_GUARD_HPP
#define MUTATION_GUARD_HPP

namespace commands {

#ifndef NDEBUG

/// @brief Returns true when the current thread is inside a command's execute() call
bool isInsideCommand();

/// @brief RAII guard that sets the inside-command flag for its lifetime.
///
/// Used internally by executeSequence() and direct command execution to mark
/// the scope where DataManager mutations are expected.
class CommandGuard {
public:
    CommandGuard();
    ~CommandGuard();

    CommandGuard(CommandGuard const &) = delete;
    CommandGuard & operator=(CommandGuard const &) = delete;
    CommandGuard(CommandGuard &&) = delete;
    CommandGuard & operator=(CommandGuard &&) = delete;

private:
    bool _previous;
};

/// @brief Check whether a DataManager mutation is happening outside a command scope.
///
/// Call this from DataManager mutating methods in debug builds. Prints a warning
/// to stderr if the mutation is not inside a command.
/// @param method_name Name of the DataManager method being called (for the warning message)
void warnIfOutsideCommand(char const * method_name);

#else

// In release builds, everything compiles away to nothing
inline bool isInsideCommand() { return true; }

class CommandGuard {
public:
    CommandGuard() = default;
};

inline void warnIfOutsideCommand(char const * /*method_name*/) {}

#endif// NDEBUG

}// namespace commands

#endif// MUTATION_GUARD_HPP
