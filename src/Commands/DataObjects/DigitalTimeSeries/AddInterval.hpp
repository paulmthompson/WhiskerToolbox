/**
 * @file AddInterval.hpp
 * @brief Command to append an interval to a DigitalIntervalSeries
 */

#ifndef ADD_INTERVAL_COMMAND_HPP
#define ADD_INTERVAL_COMMAND_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <cstdint>
#include <string>

#include <rfl/json.hpp>

namespace commands {

struct AddIntervalParams {
    std::string interval_key;
    int64_t start_frame;
    int64_t end_frame;
    bool create_if_missing = true;
};

class AddInterval : public ICommand {
public:
    explicit AddInterval(AddIntervalParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    CommandResult execute(CommandContext const & ctx) override;

private:
    AddIntervalParams _params;
};

}// namespace commands

#endif// ADD_INTERVAL_COMMAND_HPP
