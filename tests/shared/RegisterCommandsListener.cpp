/**
 * @file RegisterCommandsListener.cpp
 * @brief Catch2 event listener that registers core commands before tests run.
 *        Include this source file in any test executable that uses commands
 *        (createCommand, executeSequence, etc.) to ensure the registry is populated.
 */

#include "Commands/Core/register_core_commands.hpp"

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

namespace {

class RegisterCommandsListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const & /*info*/) override {
        commands::register_core_commands();
    }
};

}// namespace

CATCH_REGISTER_LISTENER(RegisterCommandsListener)
