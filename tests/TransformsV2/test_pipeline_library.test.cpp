/**
 * @file test_pipeline_library.test.cpp
 * @brief Unit tests for TransformsV2 pipeline library file helpers
 */

#include "TransformsV2/io/PipelineLibrary.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace Neuralyzer::Transforms::V2::Examples;

namespace {

class TempPipelineDirectory {
public:
    TempPipelineDirectory() {
        auto const stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        _path = std::filesystem::temp_directory_path() /
                ("whiskertoolbox_pipeline_library_" + std::to_string(stamp));
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

[[nodiscard]] PipelineDescriptor makeSampleDescriptor() {
    PipelineDescriptor descriptor;
    descriptor.metadata = PipelineMetadata{
            .name = "ZScore With Pre-Reductions",
            .version = "1.0"};
    descriptor.pre_reductions = std::vector<PreReductionStepDescriptor>{
            PreReductionStepDescriptor{
                    .reduction_name = "MeanValueRaw",
                    .output_key = "computed_mean"},
            PreReductionStepDescriptor{
                    .reduction_name = "StdValueRaw",
                    .output_key = "computed_std"}};
    descriptor.steps = std::vector<PipelineStepDescriptor>{
            PipelineStepDescriptor{
                    .step_id = "zscore",
                    .transform_name = "ZScoreNormalizeV2",
                    .param_bindings = std::map<std::string, std::string>{
                            {"mean", "computed_mean"},
                            {"std_dev", "computed_std"}}}};
    descriptor.range_reduction = RangeReductionStepDescriptor{
            .reduction_name = "MeanValue"};
    return descriptor;
}

}// namespace

TEST_CASE("defaultUserPipelineDirectory appends transforms_v2 segment",
          "[PipelineLibrary][path]") {
    auto const dir = defaultUserPipelineDirectory(std::filesystem::path{"/tmp/wt_config"});
    CHECK(dir == std::filesystem::path{"/tmp/wt_config/pipelines/transforms_v2"});
}

TEST_CASE("ensureUserPipelineDirectory creates nested directory",
          "[PipelineLibrary][path]") {
    TempPipelineDirectory temp;
    auto const config_dir = temp.path() / "config";

    auto const result = ensureUserPipelineDirectory(config_dir);
    REQUIRE(result);
    CHECK(std::filesystem::is_directory(result.value()));
    CHECK(result.value() == defaultUserPipelineDirectory(config_dir));
}

TEST_CASE("sanitizePipelineFilename produces safe stems", "[PipelineLibrary][sanitize]") {
    CHECK(sanitizePipelineFilename("Mask Area Default") == "mask_area_default");
    CHECK(sanitizePipelineFilename("  ") == "pipeline");
    CHECK(sanitizePipelineFilename("Already_Safe-123") == "already_safe_123");
}

TEST_CASE("save and load pipeline descriptor round-trip via file API",
          "[PipelineLibrary][io]") {
    TempPipelineDirectory temp;
    auto const filepath = temp.path() / "zscore_pipeline.json";
    auto const descriptor = makeSampleDescriptor();

    auto const save_result = savePipelineDescriptorToFile(filepath, descriptor);
    REQUIRE(save_result);

    auto const load_result = loadPipelineDescriptorFromFile(filepath);
    REQUIRE(load_result);

    auto const & loaded = load_result.value();
    REQUIRE(loaded.metadata.has_value());
    REQUIRE(loaded.metadata->name == "ZScore With Pre-Reductions");
    REQUIRE(loaded.pre_reductions.has_value());
    REQUIRE(loaded.pre_reductions->size() == 2);
    REQUIRE(loaded.steps.size() == 1);
    REQUIRE(loaded.range_reduction.has_value());
    CHECK(loaded.range_reduction->reduction_name == "MeanValue");
}

TEST_CASE("savePipelineToFile matches loadPipelineDescriptorFromJsonFile",
          "[PipelineLibrary][io]") {
    TempPipelineDirectory temp;
    auto const filepath = temp.path() / "via_loader.json";
    auto const descriptor = makeSampleDescriptor();

    REQUIRE(savePipelineToFile(filepath.string(), descriptor));

    auto const load_result = loadPipelineDescriptorFromJsonFile(filepath.string());
    REQUIRE(load_result);
    CHECK(load_result.value().steps[0].transform_name == "ZScoreNormalizeV2");
}

TEST_CASE("listPipelineFiles returns sorted entries with display names",
          "[PipelineLibrary][catalog]") {
    TempPipelineDirectory temp;
    auto const library_dir = temp.path() / "library";
    std::filesystem::create_directories(library_dir);

    PipelineDescriptor beta;
    beta.metadata = PipelineMetadata{.name = "Beta Pipeline"};
    beta.steps = {PipelineStepDescriptor{
            .step_id = "s1",
            .transform_name = "CalculateMaskArea"}};
    REQUIRE(savePipelineDescriptorToFile(library_dir / "beta.json", beta));

    PipelineDescriptor alpha;
    alpha.metadata = PipelineMetadata{.name = "Alpha Pipeline"};
    alpha.steps = {PipelineStepDescriptor{
            .step_id = "s1",
            .transform_name = "CalculateMaskArea"}};
    REQUIRE(savePipelineDescriptorToFile(library_dir / "alpha.json", alpha));

    std::ofstream ignore(library_dir / "readme.txt");
    ignore << "not a pipeline";

    auto const entries = listPipelineFiles(library_dir);
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].display_name == "Alpha Pipeline");
    CHECK(entries[1].display_name == "Beta Pipeline");
}

TEST_CASE("deletePipelineFile removes an existing file", "[PipelineLibrary][io]") {
    TempPipelineDirectory temp;
    auto const filepath = temp.path() / "to_delete.json";
    REQUIRE(savePipelineDescriptorToFile(filepath, makeSampleDescriptor()));
    REQUIRE(std::filesystem::exists(filepath));

    REQUIRE(deletePipelineFile(filepath));
    CHECK_FALSE(std::filesystem::exists(filepath));
}

TEST_CASE("loadPipelineFromFile still builds executable pipeline",
          "[PipelineLibrary][io]") {
    TempPipelineDirectory temp;
    auto const filepath = temp.path() / "executable.json";

    PipelineDescriptor descriptor;
    descriptor.steps = {PipelineStepDescriptor{
            .step_id = "area",
            .transform_name = "CalculateMaskArea"}};
    REQUIRE(savePipelineDescriptorToFile(filepath, descriptor));

    auto const pipeline_result = loadPipelineFromFile(filepath.string());
    REQUIRE(pipeline_result);
    CHECK(pipeline_result.value().size() == 1);
}
