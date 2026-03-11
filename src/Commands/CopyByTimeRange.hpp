/**
 * @file CopyByTimeRange.hpp
 * @brief Command to copy entities in a time range between data objects
 */

#ifndef COPY_BY_TIME_RANGE_HPP
#define COPY_BY_TIME_RANGE_HPP

#include "CommandResult.hpp"
#include "ICommand.hpp"

#include "Entity/EntityTypes.hpp"

#include <cstdint>
#include <string>
#include <unordered_set>

#include <rfl/json.hpp>

namespace commands {

struct CopyByTimeRangeParams {
    std::string source_key;
    std::string destination_key;
    int64_t start_frame;
    int64_t end_frame;
};

class CopyByTimeRange : public ICommand {
public:
    explicit CopyByTimeRange(CopyByTimeRangeParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    CommandResult execute(CommandContext const & ctx) override;

private:
    CopyByTimeRangeParams _params;
};

}// namespace commands

#endif// COPY_BY_TIME_RANGE_HPP
