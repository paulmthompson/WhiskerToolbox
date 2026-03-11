/**
 * @file SaveData.hpp
 * @brief Command to save a data object from DataManager to disk via the LoaderRegistry
 */

#ifndef SAVE_DATA_COMMAND_HPP
#define SAVE_DATA_COMMAND_HPP

#include "CommandResult.hpp"
#include "ICommand.hpp"

#include <optional>
#include <string>

#include <rfl.hpp>
#include <rfl/json.hpp>

namespace commands {

struct SaveDataParams {
    std::string data_key;                      ///< Key in DataManager
    std::string format;                        ///< e.g., "csv", "capnproto", "opencv"
    std::string path;                          ///< Output file/directory path
    std::optional<rfl::Generic> format_options;///< Format-specific options (optional)
};

class SaveData : public ICommand {
public:
    explicit SaveData(SaveDataParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;

    CommandResult execute(CommandContext const & ctx) override;

private:
    SaveDataParams _params;
};

}// namespace commands

#endif// SAVE_DATA_COMMAND_HPP
