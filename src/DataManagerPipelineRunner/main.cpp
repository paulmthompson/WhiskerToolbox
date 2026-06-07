/**
 * @file main.cpp
 * @brief Headless executable for running DataManager JSON pipeline configs.
 */

#include "Commands/register_core_commands.hpp"
#include "DataManager.hpp"
#include "DataSynthesizer/register_datasynthesizer_commands.hpp"
#include "JsonPipeline/JsonPipelineRunner.hpp"
#include "TransformsV2/register_transformsv2_commands.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

/**
 * @brief Parsed command-line arguments for the pipeline runner.
 */
struct CliParseResult {
    bool m_valid = false;                                              ///< True when arguments are valid.
    bool m_show_help = false;                                          ///< True when usage should be printed.
    std::string m_error;                                               ///< Error message when parsing fails.
    std::string m_config_path;                                         ///< Pipeline JSON config path.
    WhiskerToolbox::DataManagerPipeline::JsonPipelineOptions m_options;///< Runner options.
};

/**
 * @brief Print command-line usage.
 * @param executable_name Name used to invoke the executable.
 * @post Usage text is written to stdout.
 */
void printUsage(std::string_view const executable_name) {
    std::cout << "Usage: " << executable_name << " [options] <pipeline.json>\n\n"
              << "Options:\n"
              << "  --help                         Show this help text.\n"
              << "  --no-load-transform            Skip legacy data loading and transformation processing.\n"
              << "  --no-commands                  Skip root-level commands[].\n"
              << "  --continue-on-command-error    Continue executing commands after command failures.\n"
              << "  --no-saves                     Skip root-level saves[].\n"
              << "  --continue-on-save-error       Continue executing saves after save failures.\n";
}

/**
 * @brief Parse command-line arguments.
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return Parsed command-line result.
 * @pre argc must be non-negative.
 * @pre argv must contain at least argc entries.
 * @post Returned m_valid is true only when a config path or help flag was supplied.
 */
CliParseResult parseArguments(int const argc, char ** argv) {
    CliParseResult parsed;
    for (int i = 1; i < argc; ++i) {
        std::string_view const arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            parsed.m_show_help = true;
            parsed.m_valid = true;
            return parsed;
        }

        if (arg == "--no-load-transform") {
            parsed.m_options.m_run_load_and_transform_phase = false;
            continue;
        }
        if (arg == "--no-commands") {
            parsed.m_options.m_run_command_phase = false;
            continue;
        }
        if (arg == "--continue-on-command-error") {
            parsed.m_options.m_stop_on_command_error = false;
            continue;
        }
        if (arg == "--no-saves") {
            parsed.m_options.m_run_save_phase = false;
            continue;
        }
        if (arg == "--continue-on-save-error") {
            parsed.m_options.m_stop_on_save_error = false;
            continue;
        }

        if (!arg.empty() && arg.front() == '-') {
            parsed.m_error = "Unknown option: " + std::string(arg);
            return parsed;
        }

        if (!parsed.m_config_path.empty()) {
            parsed.m_error = "Multiple pipeline JSON paths were provided";
            return parsed;
        }
        parsed.m_config_path = std::string(arg);
    }

    if (parsed.m_config_path.empty()) {
        parsed.m_error = "Missing pipeline JSON path";
        return parsed;
    }

    parsed.m_valid = true;
    return parsed;
}

/**
 * @brief Print a successful pipeline summary.
 * @param result Successful pipeline result to summarize.
 * @post Summary text is written to stdout.
 */
void printSuccessSummary(WhiskerToolbox::DataManagerPipeline::JsonPipelineResult const & result) {
    std::cout << "Pipeline succeeded\n";
    std::cout << "Loaded data entries: " << result.m_loaded_data.size() << '\n';
    std::cout << "Command affected keys: " << result.m_command_affected_keys.size() << '\n';
    std::cout << "Saved outputs: " << result.m_saved_paths.size() << '\n';
    for (auto const & path: result.m_saved_paths) {
        std::cout << "  " << path << '\n';
    }
}

/**
 * @brief Print a failed pipeline summary.
 * @param result Failed pipeline result to summarize.
 * @post Summary text is written to stderr.
 */
void printFailureSummary(WhiskerToolbox::DataManagerPipeline::JsonPipelineResult const & result) {
    std::cerr << "Pipeline failed during phase '"
              << WhiskerToolbox::DataManagerPipeline::phaseToString(result.m_failed_phase)
              << "'";
    if (result.m_failed_command_index >= 0) {
        std::cerr << " at command index " << result.m_failed_command_index;
    }
    std::cerr << ": " << result.m_error_message << '\n';
}

}// namespace

/**
 * @brief Run the headless DataManager JSON pipeline executable.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code: 0 on success, nonzero on failure.
 */
int main(int argc, char ** argv) {
    auto const args = parseArguments(argc, argv);
    std::string_view const executable_name = argc > 0 ? argv[0] : "DataManagerPipelineRunner";

    if (args.m_show_help) {
        printUsage(executable_name);
        return 0;
    }

    if (!args.m_valid) {
        std::cerr << args.m_error << '\n';
        printUsage(executable_name);
        return 2;
    }

    try {
        commands::register_core_commands();
        WhiskerToolbox::DataSynthesizer::register_datasynthesizer_commands();
        WhiskerToolbox::Transforms::V2::register_transformsv2_commands();

        DataManager data_manager;
        auto const result = WhiskerToolbox::DataManagerPipeline::runJsonPipelineFile(
                data_manager,
                args.m_config_path,
                args.m_options);

        if (!result.m_success) {
            printFailureSummary(result);
            return 1;
        }

        printSuccessSummary(result);
        return 0;
    } catch (std::exception const & e) {
        std::cerr << "Unhandled exception: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Unhandled unknown exception\n";
        return 1;
    }
}
