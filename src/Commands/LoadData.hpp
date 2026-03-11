/**
 * @file LoadData.hpp
 * @brief Command to load data from disk into DataManager via the LoaderRegistry
 */

#ifndef LOAD_DATA_COMMAND_HPP
#define LOAD_DATA_COMMAND_HPP

#include "CommandResult.hpp"
#include "ICommand.hpp"

#include <optional>
#include <string>

#include <rfl.hpp>
#include <rfl/json.hpp>

namespace commands {

struct LoadDataParams {
    std::string data_key;                      ///< Key to store in DataManager
    std::string data_type;                     ///< e.g., "PointData", "LineData"
    std::string filepath;                      ///< Input file path
    std::string format;                        ///< e.g., "csv", "capnproto"
    std::optional<rfl::Generic> format_options;///< Format-specific options (optional)
};

class LoadData : public ICommand {
public:
    explicit LoadData(LoadDataParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;
    [[nodiscard]] bool isUndoable() const override;

    CommandResult execute(CommandContext const & ctx) override;
    CommandResult undo(CommandContext const & ctx) override;

private:
    LoadDataParams _params;
    bool _data_was_loaded = false;
};

}// namespace commands

#endif// LOAD_DATA_COMMAND_HPP
