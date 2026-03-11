/**
 * @file MutationGuard.cpp
 * @brief Implementation of debug-only MutationGuard infrastructure
 */

#include "MutationGuard.hpp"

#ifndef NDEBUG

#include <iostream>

namespace commands {

namespace {
thread_local bool g_inside_command = false;
}// namespace

bool isInsideCommand() {
    return g_inside_command;
}

CommandGuard::CommandGuard()
    : _previous(g_inside_command) {
    g_inside_command = true;
}

CommandGuard::~CommandGuard() {
    g_inside_command = _previous;
}

void warnIfOutsideCommand(char const * method_name) {
    if (!g_inside_command) {
        std::cerr << "[MutationGuard] DataManager::" << method_name
                  << "() called outside a command scope. "
                  << "Consider routing this mutation through a command."
                  << std::endl;
    }
}

}// namespace commands

#endif// NDEBUG
