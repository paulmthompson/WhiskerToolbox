/**
 * @file RunTransformsV2PipelineAtTime.test.cpp
 * @brief Tests for RunTransformsV2PipelineAtTime command (stub)
 */

#include "TransformsV2/Commands/RunTransformsV2PipelineAtTime.hpp"
#include "TransformsV2/Commands/RunTransformsV2PipelineAtTimeUIHints.hpp"
#include "TransformsV2/io/PipelineLibrary.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"
#include "TransformsV2/register_transformsv2_commands.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/CommandRegistry.hpp"
#include "Commands/Core/SequenceExecution.hpp"

#include "DataManager/DataManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include <rfl/json.hpp>

using namespace commands;
using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

namespace {

class TempPipelineDirectory {
public:
    TempPipelineDirectory() {
        auto const stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path() /
                ("whiskertoolbox_run_pipeline_cmd_" + std::to_string(stamp));
        std::filesystem::create_directories(_path);
    }

    ~TempPipelineDirectory() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    [[nodiscard]] std::filesystem::path const & path() const { return _path; }

private:
    std::filesystem::path _path;
};

[[nodiscard]] PipelineDescriptor makeMinimalDescriptor() {
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{.name = "Test Pipeline", .version = "1.0"};
    descriptor.steps = std::vector<PipelineStepDescriptor>{
            PipelineStepDescriptor{
                    .step_id = "mask_area",
                    .transform_name = "MaskArea",
                    .param_bindings = {}}};
    return descriptor;
}

[[nodiscard]] std::filesystem::path writePipelineJson(TempPipelineDirectory const & temp) {
    auto const filepath = temp.path() / "test_pipeline.json";
    auto const save_result = savePipelineDescriptorToFile(filepath, makeMinimalDescriptor());
    REQUIRE(save_result);
    return filepath;
}

}// namespace

TEST_CASE("RunTransformsV2PipelineAtTime is registered after register_transformsv2_commands",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    auto & reg = CommandRegistry::instance();
    REQUIRE(reg.isKnown("RunTransformsV2PipelineAtTime"));

    auto const info = reg.commandInfo("RunTransformsV2PipelineAtTime");
    REQUIRE(info.has_value());
    CHECK(info->category == "transforms");
    CHECK_FALSE(info->supports_undo);
}

TEST_CASE("RunTransformsV2PipelineAtTime parameter schema has pipeline path hints",
          "[transforms][commands][RunTransformsV2PipelineAtTime][schema]") {
    auto const schema = extractParameterSchema<RunTransformsV2PipelineAtTimeParams>();

    auto const * pipeline_path = schema.field("pipeline_path");
    REQUIRE(pipeline_path != nullptr);
    CHECK(pipeline_path->path_field_kind == PathFieldKind::FilePath);
    CHECK(pipeline_path->file_dialog_id == "transformv2_pipeline_open");

    auto const * input_key = schema.field("input_key");
    REQUIRE(input_key != nullptr);
    CHECK(input_key->dynamic_combo);

    auto const * time_field = schema.field("time");
    REQUIRE(time_field != nullptr);
    CHECK(time_field->type_name == "int");
}

TEST_CASE("RunTransformsV2PipelineAtTime stub execute validates pipeline_path",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{});
    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();

    auto const empty_path = cmd.execute(ctx);
    REQUIRE_FALSE(empty_path.success);
    CHECK(empty_path.error_message.find("pipeline_path") != std::string::npos);
}

TEST_CASE("RunTransformsV2PipelineAtTime stub execute succeeds with valid pipeline file",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    TempPipelineDirectory temp;
    auto const filepath = writePipelineJson(temp);

    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{
            .input_key = "input_masks",
            .output_key = "output_masks",
            .time = 42,
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();

    auto const result = cmd.execute(ctx);
    REQUIRE(result.success);
}

TEST_CASE("createCommand deserializes RunTransformsV2PipelineAtTime",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    auto const json = R"({
        "input_key": "src",
        "output_key": "dst",
        "time": 42,
        "pipeline_path": "/tmp/pipeline.json"
    })";
    auto const generic = rfl::json::read<rfl::Generic>(json).value();
    auto cmd = createCommand("RunTransformsV2PipelineAtTime", generic);
    REQUIRE(cmd != nullptr);
    CHECK(cmd->commandName() == "RunTransformsV2PipelineAtTime");
    CHECK_FALSE(cmd->isUndoable());
}

TEST_CASE("executeSequence substitutes current_frame into RunTransformsV2PipelineAtTime",
          "[transforms][commands][RunTransformsV2PipelineAtTime][variable-substitution]") {
    TempPipelineDirectory temp;
    auto const filepath = writePipelineJson(temp);

    CommandSequenceDescriptor seq;
    CommandDescriptor step;
    step.command_name = "RunTransformsV2PipelineAtTime";
    step.parameters = rfl::json::read<rfl::Generic>(
                              R"({
                                  "input_key": "src",
                                  "output_key": "dst",
                                  "time": "${current_frame}",
                                  "pipeline_path": ")" +
                              filepath.string() + R"("
                              })")
                              .value();
    seq.commands = {step};

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    ctx.runtime_variables = {{"current_frame", "9"}};

    auto const out = executeSequence(seq, ctx);
    REQUIRE(out.result.success);
}
