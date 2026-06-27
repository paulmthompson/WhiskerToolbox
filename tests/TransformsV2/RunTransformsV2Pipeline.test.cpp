/**
 * @file RunTransformsV2Pipeline.test.cpp
 * @brief Tests for RunTransformsV2Pipeline command
 */

#include "TransformsV2/Commands/RunTransformsV2Pipeline.hpp"
#include "TransformsV2/Commands/RunTransformsV2PipelineUIHints.hpp"
#include "TransformsV2/algorithms/LineResample/LineResample.hpp"
#include "TransformsV2/algorithms/LineSubsegment/LineSubsegment.hpp"
#include "TransformsV2/algorithms/SincInterpolation/SincInterpolation.hpp"
#include "TransformsV2/io/PipelineLibrary.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"
#include "TransformsV2/register_transformsv2_commands.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandFactory.hpp"
#include "Commands/Core/CommandRegistry.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "fixtures/builders/LineDataBuilder.hpp"
#include "fixtures/scenarios/line/resample_scenarios.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <filesystem>
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
                ("whiskertoolbox_run_full_pipeline_cmd_" + std::to_string(stamp));
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

[[nodiscard]] PipelineDescriptor makeSincInterpolationDescriptor() {
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{.name = "Sinc Upsample", .version = "1.0"};
    descriptor.steps = std::vector<PipelineStepDescriptor>{
            PipelineStepDescriptor{
                    .step_id = "upsample",
                    .transform_name = "SincInterpolation",
                    .parameters =
                            rfl::json::read<rfl::Generic>(R"({"upsampling_factor": 4})").value(),
                    .param_bindings = {}}};
    return descriptor;
}

[[nodiscard]] std::filesystem::path writePipelineJson(
        TempPipelineDirectory const & temp,
        PipelineDescriptor const & descriptor) {
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

void setupSincInterpolationInput(DataManager & dm) {
    std::vector<int> const times = {0, 100, 200, 300, 400};
    auto const source_tf = std::make_shared<TimeFrame>(times);
    dm.setTime(TimeKey("source_time"), source_tf);

    std::vector<int> upsampled_times;
    upsampled_times.reserve(17);
    for (int i = 0; i < 17; ++i) {
        upsampled_times.push_back(i * 25);
    }
    auto const upsampled_tf = std::make_shared<TimeFrame>(upsampled_times);
    dm.setTime(TimeKey("upsampled_time"), upsampled_tf);

    std::vector<float> const data = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    auto const input = std::make_shared<AnalogTimeSeries>(std::vector<float>(data), data.size());
    dm.setData<AnalogTimeSeries>("ramp", input, TimeKey("source_time"));
}

}// namespace

TEST_CASE("RunTransformsV2Pipeline is registered after register_transformsv2_commands",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    auto & reg = CommandRegistry::instance();
    REQUIRE(reg.isKnown("RunTransformsV2Pipeline"));

    auto const info = reg.commandInfo("RunTransformsV2Pipeline");
    REQUIRE(info.has_value());
    CHECK(info.value().category == "transforms");// NOLINT(bugprone-unchecked-optional-access)
    CHECK_FALSE(info.value().supports_undo);// NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("RunTransformsV2Pipeline parameter schema has expected hints",
          "[transforms][commands][RunTransformsV2Pipeline][schema]") {
    auto const schema = extractParameterSchema<RunTransformsV2PipelineParams>();

    auto const * pipeline_path = schema.field("pipeline_path");
    REQUIRE(pipeline_path != nullptr);
    CHECK(pipeline_path->path_field_kind == PathFieldKind::FilePath);
    CHECK(pipeline_path->file_dialog_id == "transformv2_pipeline_open");

    auto const * input_key = schema.field("input_key");
    REQUIRE(input_key != nullptr);
    CHECK(input_key->dynamic_combo);

    auto const * output_time_key = schema.field("output_time_key");
    REQUIRE(output_time_key != nullptr);
    CHECK_FALSE(output_time_key->tooltip.empty());
}

TEST_CASE("RunTransformsV2Pipeline execute validates pipeline_path",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "input_masks",
            .output_key = "output_masks",
            .pipeline_path = ""});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);

    auto const result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    CHECK(result.error_message.find("pipeline_path") != std::string::npos);
}

TEST_CASE("RunTransformsV2Pipeline execute rejects missing input_key",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeMinimalDescriptor());

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "missing_masks",
            .output_key = "output_masks",
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();

    auto const result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    CHECK(result.error_message.find("missing_masks") != std::string::npos);
}

TEST_CASE("RunTransformsV2Pipeline execute rejects unknown output_time_key",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeMinimalDescriptor());

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "input_masks",
            .output_key = "output_masks",
            .pipeline_path = filepath.string(),
            .output_time_key = "missing_time"});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);

    auto const result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    CHECK(result.error_message.find("missing_time") != std::string::npos);
}

TEST_CASE("RunTransformsV2Pipeline executes CalculateMaskArea on full dataset",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeMinimalDescriptor());

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "input_masks",
            .output_key = "mask_areas",
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    auto const areas = ctx.data_manager->getData<RaggedAnalogTimeSeries>("mask_areas");
    REQUIRE(areas);
    REQUIRE(areas->getNumTimePoints() == 5);
    for (int i = 0; i < 5; ++i) {
        auto const values = areas->getValuesAtTimeVec(TimeFrameIndex(i));
        REQUIRE(values.size() == 1);
        CHECK(values[0] == static_cast<float>(i + 1));
    }
    CHECK(ctx.data_manager->getTimeKey("mask_areas").str() == "camera_time");
}

TEST_CASE("RunTransformsV2Pipeline executes multi-step line pipeline on full dataset",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeResampleSubsegmentDescriptor());

    auto dm = std::make_shared<DataManager>();
    auto const time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);

    auto const input_lines = resample_scenarios::two_diagonal_lines();
    input_lines->setTimeFrame(time_frame);
    dm->setData("input_lines", input_lines, TimeKey("default"));

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "input_lines",
            .output_key = "output_lines",
            .pipeline_path = filepath.string()});

    CommandContext ctx;
    ctx.data_manager = dm;

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    auto const output = dm->getData<LineData>("output_lines");
    REQUIRE(output);
    REQUIRE(output->getAtTime(TimeFrameIndex(100)).size() == 1);
    REQUIRE(output->getAtTime(TimeFrameIndex(200)).size() == 1);

    LineResampleParams resample_params;
    resample_params.method = LineResampleMethod::FixedSpacing;
    resample_params.target_spacing = 15.0f;

    LineSubsegmentParams subsegment_params;
    subsegment_params.start_position = 0.2f;
    subsegment_params.end_position = 0.8f;
    subsegment_params.method = LineSubsegmentMethod::Direct;
    subsegment_params.preserve_original_spacing = true;

    for (auto const time_index: {TimeFrameIndex(100), TimeFrameIndex(200)}) {
        Line2D const input_line = get_first_line_at(input_lines.get(), time_index);
        Line2D const resampled = resampleLine(input_line, resample_params);
        Line2D const expected = extractLineSubsegment(resampled, subsegment_params);
        Line2D const actual = get_first_line_at(output.get(), time_index);
        REQUIRE(actual.size() == expected.size());
        CHECK(!actual.empty());
    }
}

TEST_CASE("RunTransformsV2Pipeline replaces existing output key",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeMinimalDescriptor());

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);

    auto const stale_output = std::make_shared<RaggedAnalogTimeSeries>();
    stale_output->setTimeFrame(ctx.data_manager->getTime(TimeKey("camera_time")));
    stale_output->setDataAtTime(
            TimeFrameIndex(0), std::vector<float>{99.0f}, NotifyObservers::No);
    ctx.data_manager->setData<RaggedAnalogTimeSeries>(
            "mask_areas", stale_output, TimeKey("camera_time"));

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "input_masks",
            .output_key = "mask_areas",
            .pipeline_path = filepath.string()});

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    auto const areas = ctx.data_manager->getData<RaggedAnalogTimeSeries>("mask_areas");
    REQUIRE(areas);
    REQUIRE(areas->getNumTimePoints() == 5);
    CHECK(areas->getValuesAtTimeVec(TimeFrameIndex(0))[0] == 1.0f);
}

TEST_CASE("RunTransformsV2Pipeline SincInterpolation uses output_time_key",
          "[transforms][commands][RunTransformsV2Pipeline][sinc]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeSincInterpolationDescriptor());

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupSincInterpolationInput(*ctx.data_manager);

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "ramp",
            .output_key = "ramp_upsampled",
            .pipeline_path = filepath.string(),
            .output_time_key = "upsampled_time"});

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    auto const output = ctx.data_manager->getData<AnalogTimeSeries>("ramp_upsampled");
    REQUIRE(output);
    REQUIRE(output->getNumSamples() == 17);
    CHECK(ctx.data_manager->getTimeKey("ramp_upsampled").str() == "upsampled_time");

    std::vector<float> const expected = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    auto const out = output->getAnalogTimeSeries();
    for (size_t i = 0; i < expected.size(); ++i) {
        size_t const out_idx = i * 4;
        INFO("Original sample " << i << " at output index " << out_idx);
        REQUIRE_THAT(out[out_idx], Catch::Matchers::WithinAbs(expected[i], 1e-6f));
    }
}

TEST_CASE("RunTransformsV2Pipeline SincInterpolation inherits input TimeKey when output_time_key empty",
          "[transforms][commands][RunTransformsV2Pipeline][sinc]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeSincInterpolationDescriptor());

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupSincInterpolationInput(*ctx.data_manager);

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "ramp",
            .output_key = "ramp_upsampled",
            .pipeline_path = filepath.string()});

    auto const result = cmd.execute(ctx);
    INFO("Error: " << result.error_message);
    REQUIRE(result.success);

    auto const output = ctx.data_manager->getData<AnalogTimeSeries>("ramp_upsampled");
    REQUIRE(output);
    REQUIRE(output->getNumSamples() == 17);
    CHECK(ctx.data_manager->getTimeKey("ramp_upsampled").str() == "source_time");
}

TEST_CASE("RunTransformsV2Pipeline rejects type mismatch on existing output key",
          "[transforms][commands][RunTransformsV2Pipeline]") {
    TempPipelineDirectory const temp;
    auto const filepath = writePipelineJson(temp, makeMinimalDescriptor());

    CommandContext ctx;
    ctx.data_manager = std::make_shared<DataManager>();
    setupMaskInputData(*ctx.data_manager);
    ctx.data_manager->setData<LineData>(
            "mask_areas",
            std::make_shared<LineData>(),
            TimeKey("camera_time"));

    RunTransformsV2Pipeline cmd(RunTransformsV2PipelineParams{
            .input_key = "input_masks",
            .output_key = "mask_areas",
            .pipeline_path = filepath.string()});

    auto const result = cmd.execute(ctx);
    REQUIRE_FALSE(result.success);
    CHECK(result.error_message.find("different data type") != std::string::npos);
}
