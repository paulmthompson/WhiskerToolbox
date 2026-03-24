#include <catch2/catch_test_macros.hpp>

#include "mask_cleaning.hpp"
#include "Masks/Mask_Data.hpp"

#include "DataManager.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "transforms/ParameterFactory.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "fixtures/scenarios/mask/connected_component_scenarios.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("MaskCleaning - largest single component after per-frame merge",
          "[mask_cleaning][scenario]") {
    auto mask_data = mask_scenarios::large_and_small_components();
    auto params = std::make_unique<MaskCleaningParameters>();
    params->selection = MaskCleaningSelection::Largest;
    params->count = 1;

    auto result = mask_cleaning(mask_data.get(), params.get());
    REQUIRE(result != nullptr);

    auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);
    REQUIRE(result_masks[0].size() == 9);
}

TEST_CASE("MaskCleaning - smallest single component after per-frame merge",
          "[mask_cleaning][scenario]") {
    auto mask_data = mask_scenarios::large_and_small_components();
    auto params = std::make_unique<MaskCleaningParameters>();
    params->selection = MaskCleaningSelection::Smallest;
    params->count = 1;

    auto result = mask_cleaning(mask_data.get(), params.get());
    REQUIRE(result != nullptr);

    auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);
    REQUIRE(result_masks[0].size() == 1);
}

TEST_CASE("MaskCleaning - two largest on mixed frame",
          "[mask_cleaning][scenario]") {
    auto mask_data = mask_scenarios::json_pipeline_mixed();
    auto params = std::make_unique<MaskCleaningParameters>();
    params->selection = MaskCleaningSelection::Largest;
    params->count = 2;

    auto result = mask_cleaning(mask_data.get(), params.get());
    REQUIRE(result != nullptr);

    auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);
    size_t total_pixels = 0;
    for (auto const & m : result_masks) {
        total_pixels += m.size();
    }
    REQUIRE(total_pixels == 13);
}

TEST_CASE("MaskCleaning - two smallest on mixed frame",
          "[mask_cleaning][scenario]") {
    auto mask_data = mask_scenarios::json_pipeline_mixed();
    auto params = std::make_unique<MaskCleaningParameters>();
    params->selection = MaskCleaningSelection::Smallest;
    params->count = 2;

    auto result = mask_cleaning(mask_data.get(), params.get());
    REQUIRE(result != nullptr);

    auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);
    size_t total_pixels = 0;
    for (auto const & m : result_masks) {
        total_pixels += m.size();
    }
    REQUIRE(total_pixels == 5);
}

TEST_CASE("MaskCleaningOperation - name and defaults",
          "[mask_cleaning][operation]") {
    MaskCleaningOperation op;
    REQUIRE(op.getName() == "Mask Cleaning");
    REQUIRE(op.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));

    auto defaults = op.getDefaultParameters();
    REQUIRE(defaults != nullptr);
    auto * p = dynamic_cast<MaskCleaningParameters *>(defaults.get());
    REQUIRE(p != nullptr);
    REQUIRE(p->selection == MaskCleaningSelection::Largest);
    REQUIRE(p->count == 1);
}

TEST_CASE("Data Transform: Mask Cleaning - JSON pipeline",
          "[transforms][mask_cleaning][json][scenario]") {
    auto dm = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>();
    dm->setTime(TimeKey("default"), time_frame);

    auto mask_data = mask_scenarios::json_pipeline_mixed();
    mask_data->setTimeFrame(time_frame);
    dm->setData("json_pipeline_mixed", mask_data, TimeKey("default"));

    char const * json_config =
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"Mask Cleaning Pipeline\",\n"
            "            \"description\": \"Test\",\n"
            "            \"version\": \"1.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"Mask Cleaning\",\n"
            "                \"phase\": \"analysis\",\n"
            "                \"input_key\": \"json_pipeline_mixed\",\n"
            "                \"output_key\": \"cleaned_mask\",\n"
            "                \"parameters\": {\n"
            "                    \"selection\": \"Largest\",\n"
            "                    \"count\": 2\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";

    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "mask_cleaning_pipeline_test";
    std::filesystem::create_directories(test_dir);
    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
    }

    auto data_info_list = load_data_from_json_config(dm.get(), json_filepath.string());
    static_cast<void>(data_info_list);

    auto result_mask = dm->getData<MaskData>("cleaned_mask");
    REQUIRE(result_mask != nullptr);

    auto const & result_masks = result_mask->getAtTime(TimeFrameIndex(0));
    REQUIRE(result_masks.size() == 1);
    size_t total_pixels = 0;
    for (auto const & m : result_masks) {
        total_pixels += m.size();
    }
    REQUIRE(total_pixels == 13);

    try {
        std::filesystem::remove_all(test_dir);
    } catch (std::exception const & e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}
