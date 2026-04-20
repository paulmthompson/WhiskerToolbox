/**
 * @file AdvanceFrame.hpp
 * @brief Command to shift the current visualization frame by a signed delta
 */

#ifndef ADVANCE_FRAME_COMMAND_HPP
#define ADVANCE_FRAME_COMMAND_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <cstdint>

#include <rfl/json.hpp>

namespace commands {

struct AdvanceFrameParams {
    int64_t delta = 1;
};

/**
 * @brief Advances (or rewinds) the current `TimePosition` on `CommandContext::time_controller`.
 */
class AdvanceFrame : public ICommand {
public:
    explicit AdvanceFrame(AdvanceFrameParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    CommandResult execute(CommandContext const & ctx) override;

private:
    AdvanceFrameParams _params;
};

}// namespace commands

#endif// ADVANCE_FRAME_COMMAND_HPP
