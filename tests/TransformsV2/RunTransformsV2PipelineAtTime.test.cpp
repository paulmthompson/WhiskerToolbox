/**
 * @file RunTransformsV2PipelineAtTime.test.cpp
 * @brief Tests for RunTransformsV2PipelineAtTime command
 */

#include "TransformsV2/Commands/RunTransformsV2PipelineAtTime.hpp"
#include "TransformsV2/Commands/RunTransformsV2PipelineAtTimeUIHints.hpp"
#include "TransformsV2/algorithms/LineResample/LineResample.hpp"
#include "TransformsV2/algorithms/LineSubsegment/LineSubsegment.hpp"
#include "TransformsV2/io/PipelineLibrary.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"
#include "TransformsV2/register_transformsv2_commands.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/CommandRegistry.hpp"
#include "Commands/Core/SequenceExecution.hpp"

#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "fixtures/builders/LineDataBuilder.hpp"
#include "fixtures/scenarios/line/resample_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

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

[[nodiscard]] Mask2D make_mask(int point_count) {
    std::vector<Point2D<uint32_t>> points;
    points.reserve(static_cast<size_t>(point_count));
    for (int i = 0; i < point_count; ++i) {
        points.emplace_back(static_cast<uint32_t>(i), 0u);
    }
    return Mask2D(points);
}

[[nodiscard]] PipelineDescriptor makeMinimalDescriptor() {
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{.name = "Test Pipeline", .version = "1.0"};
    descriptor.steps = std::vector<PipelineStepDescriptor>{
            PipelineStepDescriptor{
                    .step_id = "mask_area",
                    .transform_name = "CalculateMaskArea",
                    .param_bindings = {}}};
    return descriptor;
}

[[nodiscard]] std::filesystem::path writePipelineJson(
        TempPipelineDirectory const & temp,
        PipelineDescriptor const & descriptor = makeMinimalDescriptor()) {
    auto const filepath = temp.path() / "test_pipeline.json";
    auto const save_result = savePipelineDescriptorToFile(filepath, descriptor);
    REQUIRE(save_result);
    return filepath;
}

[[nodiscard]] Line2D get_first_line_at(LineData const * line_data, TimeFrameIndex time) {
    for (auto const & line: line_data->getAtTime(time)) {
        return line;
    }
    return Line2D{};
}

void setupMaskInputData(DataManager & dm) {
    auto const time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 100, 200, 300, 400});
    dm.setTime(TimeKey("camera_time"), time_frame);

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(time_frame);
    for (int i = 0; i < 5; ++i) {
        mask_data->addAtTime(TimeFrameIndex(i), make_mask(i + 1), NotifyObservers::No);
    }
    dm.setData<MaskData>("input_masks", mask_data, TimeKey("camera_time"));
}

[[nodiscard]] PipelineDescriptor makeResampleSubsegmentDescriptor() {
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{.name = "Resample Subsegment", .version = "1.0"};
    descriptor.steps = std::vector<PipelineStepDescriptor>{
            PipelineStepDescriptor{
                    .step_id = "resample",
                    .transform_name = "ResampleLine",
                    .parameters = rfl::json::read<rfl::Generic>(
                                          R"({
                                "method": "FixedSpacing",
                                "target_spacing": 15.0,
                                "epsilon": 2.0,
                                "polynomial_order": 3
                            })")
                                          .value(),
                    .param_bindings = {}},
            PipelineStepDescriptor{
                    .step_id = "subsegment",
                    .transform_name = "ExtractLineSubsegment",
                    .parameters = rfl::json::read<rfl::Generic>(
                                          R"({
                                "start_position": 0.2,
                                "end_position": 0.8,
                                "method": "Direct",
                                "preserve_original_spacing": true
                            })")
                                          .value(),
                    .param_bindings = {}}};
    return descriptor;
}

[[nodiscard]] PipelineDescriptor makeResampleOnlyDescriptor() {
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{.name = "Resample Only", .version = "1.0"};
    descriptor.steps = std::vector<PipelineStepDescriptor>{
            PipelineStepDescriptor{
                    .step_id = "resample",
                    .transform_name = "ResampleLine",
                    .parameters = rfl::json::read<rfl::Generic>(
                                          R"({
                                "method": "FixedSpacing",
                                "target_spacing": 15.0,
                                "epsilon": 2.0,
                                "polynomial_order": 3
                            })")
                                          .value(),
                    .param_bindings = {}}};
    return descriptor;
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

TEST_CASE("RunTransformsV2PipelineAtTime execute validates pipeline_path",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{
            .input_key = "input_masks",
            .output_key = "output_masks",
            .time = 0,
            .pipeline_path = ""});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);

    auto const empty_path = cmd.execute(ctx);
    REQUIRE_FALSE(empty_path.success);
    CHECK(empty_path.error_message.find("pipeline_path") != std::string::npos);
}

TEST_CASE("RunTransformsV2PipelineAtTime execute rejects empty input_key",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp);

    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{
            .input_key = "",
            .output_key = "output_masks",
            .time = 0,
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();

    auto const result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    CHECK(result.error_message.find("input_key") != std::string::npos);
}

TEST_CASE("RunTransformsV2PipelineAtTime execute rejects missing input_key",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp);

    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{
            .input_key = "missing_masks",
            .output_key = "output_masks",
            .time = 0,
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();

    auto const result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    CHECK(result.error_message.find("missing_masks") != std::string::npos);
}

TEST_CASE("RunTransformsV2PipelineAtTime executes CalculateMaskArea at one frame",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp);

    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{
            .input_key = "input_masks",
            .output_key = "mask_areas",
            .time = 0,
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);
    REQUIRE(result.affected_keys.size() == 1);
    CHECK(result.affected_keys[0] == "mask_areas");

    auto const areas = ctx.data_manager->getData<RaggedAnalogTimeSeries>("mask_areas");
    REQUIRE(areas);
    REQUIRE(areas->getNumTimePoints() == 1);
    REQUIRE(areas->getValuesAtTimeVec(TimeFrameIndex(0)).size() == 1);
    CHECK(areas->getValuesAtTimeVec(TimeFrameIndex(0))[0] == 1.0f);
}

TEST_CASE("RunTransformsV2PipelineAtTime executes multi-step line pipeline at one frame",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeResampleSubsegmentDescriptor());

    auto dm = std::make_shared<DataManager>();
    auto const time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);

    auto const input_lines = resample_scenarios::two_diagonal_lines();
    input_lines->setTimeFrame(time_frame);
    dm->setData("input_lines", input_lines, TimeKey("default"));

    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{
            .input_key = "input_lines",
            .output_key = "output_lines",
            .time = 100,
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = dm;

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    auto const output = dm->getData<LineData>("output_lines");
    REQUIRE(output);
    REQUIRE(output->getAtTime(TimeFrameIndex(100)).size() == 1);

    LineResampleParams resample_params;
    resample_params.method = LineResampleMethod::FixedSpacing;
    resample_params.target_spacing = 15.0f;

    LineSubsegmentParams subsegment_params;
    subsegment_params.start_position = 0.2f;
    subsegment_params.end_position = 0.8f;
    subsegment_params.method = LineSubsegmentMethod::Direct;
    subsegment_params.preserve_original_spacing = true;

    Line2D const input_at_100 = get_first_line_at(input_lines.get(), TimeFrameIndex(100));
    Line2D const resampled = resampleLine(input_at_100, resample_params);
    Line2D const expected = extractLineSubsegment(resampled, subsegment_params);
    Line2D const actual = get_first_line_at(output.get(), TimeFrameIndex(100));

    REQUIRE(actual.size() == expected.size());
    CHECK(!actual.empty());
}

TEST_CASE("RunTransformsV2PipelineAtTime merge-overwrites existing LineData output",
          "[transforms][commands][RunTransformsV2PipelineAtTime]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeResampleOnlyDescriptor());

    auto dm = std::make_shared<DataManager>();
    auto const time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);

    auto const input_lines = resample_scenarios::two_diagonal_lines();
    input_lines->setTimeFrame(time_frame);
    dm->setData("input_lines", input_lines, TimeKey("default"));

    auto const existing = std::make_shared<LineData>();
    existing->setTimeFrame(time_frame);
    existing->addAtTime(
            TimeFrameIndex(100),
            line_shapes::horizontal(0.0f, 100.0f, 5.0f, 20),
            NotifyObservers::No);
    existing->addAtTime(
            TimeFrameIndex(300),
            line_shapes::horizontal(0.0f, 50.0f, 25.0f, 4),
            NotifyObservers::No);
    dm->setData<LineData>("output_lines", existing, TimeKey("default"));

    RunTransformsV2PipelineAtTime cmd(RunTransformsV2PipelineAtTimeParams{
            .input_key = "input_lines",
            .output_key = "output_lines",
            .time = 100,
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = dm;

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    auto const output = dm->getData<LineData>("output_lines");
    REQUIRE(output.get() == existing.get());

    Line2D const preserved_at_300 = get_first_line_at(output.get(), TimeFrameIndex(300));
    CHECK(preserved_at_300.size() == 4);

    LineResampleParams params;
    params.method = LineResampleMethod::FixedSpacing;
    params.target_spacing = 15.0f;

    Line2D const input_at_100 = get_first_line_at(input_lines.get(), TimeFrameIndex(100));
    Line2D const expected_at_100 = resampleLine(input_at_100, params);
    Line2D const merged_at_100 = get_first_line_at(output.get(), TimeFrameIndex(100));
    REQUIRE(merged_at_100.size() == expected_at_100.size());
    CHECK(merged_at_100.size() < 20);
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
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp);

    CommandSequenceDescriptor seq;
    CommandDescriptor step;
    step.command_name = "RunTransformsV2PipelineAtTime";
    step.parameters = rfl::json::read<rfl::Generic>(
                              R"({
                                  "input_key": "input_masks",
                                  "output_key": "dst",
                                  "time": "${current_frame}",
                                  "pipeline_path": ")" +
                              filepath.string() + R"("
                              })")
                              .value();
    seq.commands = {step};

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);
    ctx.runtime_variables = {{"current_frame", "0"}};

    auto const out = executeSequence(seq, ctx);
    INFO("Error: " << out.result.error_message);
    REQUIRE(out.result.success);
}
