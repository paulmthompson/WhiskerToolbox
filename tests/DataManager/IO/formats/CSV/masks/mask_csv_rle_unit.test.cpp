/**
 * @file mask_csv_rle_unit.test.cpp
 * @brief Deterministic round-trip unit tests for MaskData CSV RLE save/load
 *
 * Tests include:
 * 1. Single mask round-trip
 * 2. Multiple frames round-trip
 * 3. Empty MaskData save returns true
 * 4. Large pixel coordinates round-trip
 * 5. Custom RLE delimiter round-trip
 * 6. CSV file contents verification
 */

#include <catch2/catch_test_macros.hpp>

#include "IO/formats/CSV/mask/Mask_Data_CSV.hpp"
#include "Masks/Mask_Data.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

class MaskCSVRLEUnitTestFixture {
public:
    MaskCSVRLEUnitTestFixture() {
        test_dir = std::filesystem::current_path() / "test_mask_csv_rle_unit_output";
        std::filesystem::create_directories(test_dir);
        csv_filename = "test_mask_rle.csv";
        csv_filepath = test_dir / csv_filename;
    }

    ~MaskCSVRLEUnitTestFixture() {
        cleanup();
    }

protected:
    void cleanup() {
        try {
            if (std::filesystem::exists(test_dir)) {
                std::filesystem::remove_all(test_dir);
            }
        } catch (std::exception const & e) {
            // Ignore cleanup errors in destructor
            static_cast<void>(e);
        }
    }

    [[nodiscard]] CSVMaskRLESaverOptions defaultSaveOpts() const {
        CSVMaskRLESaverOptions opts;
        opts.filename = csv_filename;
        opts.parent_dir = test_dir.string();
        opts.delimiter = ",";
        opts.rle_delimiter = ",";
        opts.save_header = true;
        opts.header = "Frame,RLE";
        return opts;
    }

    [[nodiscard]] CSVMaskRLELoaderOptions defaultLoadOpts() const {
        CSVMaskRLELoaderOptions opts;
        opts.filepath = csv_filepath.string();
        opts.delimiter = ",";
        opts.rle_delimiter = ",";
        opts.has_header = true;
        opts.header_identifier = "Frame";
        return opts;
    }

    /// Compare two masks by sorting points (y,x) and checking equality
    static void verifyMaskEquality(Mask2D const & original, Mask2D const & loaded) {
        REQUIRE(loaded.size() == original.size());

        auto orig_pts = original.points();
        auto loaded_pts = loaded.points();
        auto cmp = [](auto const & a, auto const & b) {
            return (a.y < b.y) || (a.y == b.y && a.x < b.x);
        };
        std::sort(orig_pts.begin(), orig_pts.end(), cmp);
        std::sort(loaded_pts.begin(), loaded_pts.end(), cmp);

        for (size_t i = 0; i < orig_pts.size(); ++i) {
            CHECK(loaded_pts[i].x == orig_pts[i].x);
            CHECK(loaded_pts[i].y == orig_pts[i].y);
        }
    }

protected:
    std::filesystem::path test_dir;
    std::string csv_filename;
    std::filesystem::path csv_filepath;
};

TEST_CASE_METHOD(MaskCSVRLEUnitTestFixture,
                 "DM - IO - MaskData - CSV RLE save and load round-trip",
                 "[MaskData][CSV][RLE][IO][unit]") {

    SECTION("Single mask round-trip") {
        auto mask_data = std::make_shared<MaskData>();
        Mask2D mask({{10, 5}, {11, 5}, {12, 5}});
        mask_data->addAtTime(TimeFrameIndex(0), std::move(mask), NotifyObservers::No);

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);
        REQUIRE(std::filesystem::exists(csv_filepath));

        auto loaded_map = load(defaultLoadOpts());
        REQUIRE(loaded_map.size() == 1);
        REQUIRE(loaded_map.count(TimeFrameIndex(0)) == 1);
        REQUIRE(loaded_map[TimeFrameIndex(0)].size() == 1);

        Mask2D const original_mask({{10, 5}, {11, 5}, {12, 5}});
        verifyMaskEquality(original_mask, loaded_map[TimeFrameIndex(0)][0]);
    }

    SECTION("Multiple frames round-trip") {
        auto mask_data = std::make_shared<MaskData>();

        Mask2D mask0({{0, 0}, {1, 0}, {2, 0}});
        mask_data->addAtTime(TimeFrameIndex(0), std::move(mask0), NotifyObservers::No);

        Mask2D mask5({{5, 10}, {6, 10}});
        mask_data->addAtTime(TimeFrameIndex(5), std::move(mask5), NotifyObservers::No);

        Mask2D mask100({{100, 200}});
        mask_data->addAtTime(TimeFrameIndex(100), std::move(mask100), NotifyObservers::No);

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);

        auto loaded_map = load(defaultLoadOpts());
        REQUIRE(loaded_map.size() == 3);

        // Frame 0: 3 pixels
        REQUIRE(loaded_map[TimeFrameIndex(0)].size() == 1);
        CHECK(loaded_map[TimeFrameIndex(0)][0].size() == 3);

        // Frame 5: 2 pixels
        REQUIRE(loaded_map[TimeFrameIndex(5)].size() == 1);
        CHECK(loaded_map[TimeFrameIndex(5)][0].size() == 2);

        // Frame 100: 1 pixel
        REQUIRE(loaded_map[TimeFrameIndex(100)].size() == 1);
        CHECK(loaded_map[TimeFrameIndex(100)][0].size() == 1);
        CHECK(loaded_map[TimeFrameIndex(100)][0][0].x == 100);
        CHECK(loaded_map[TimeFrameIndex(100)][0][0].y == 200);
    }

    SECTION("Empty MaskData save returns true") {
        auto mask_data = std::make_shared<MaskData>();

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);
        REQUIRE(std::filesystem::exists(csv_filepath));

        // Loading should return empty map (only header in the file)
        auto loaded_map = load(defaultLoadOpts());
        CHECK(loaded_map.empty());
    }

    SECTION("Large pixel coordinates round-trip") {
        auto mask_data = std::make_shared<MaskData>();
        Mask2D mask({{500, 400}, {501, 400}, {502, 400}, {800, 900}, {801, 900}, {802, 900}, {803, 900}});
        mask_data->addAtTime(TimeFrameIndex(42), std::move(mask), NotifyObservers::No);

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);

        auto loaded_map = load(defaultLoadOpts());
        REQUIRE(loaded_map.size() == 1);
        REQUIRE(loaded_map[TimeFrameIndex(42)].size() == 1);

        Mask2D const expected({{500, 400}, {501, 400}, {502, 400}, {800, 900}, {801, 900}, {802, 900}, {803, 900}});
        verifyMaskEquality(expected, loaded_map[TimeFrameIndex(42)][0]);
    }

    SECTION("No-header save and load round-trip") {
        auto mask_data = std::make_shared<MaskData>();
        Mask2D mask({{3, 7}, {4, 7}});
        mask_data->addAtTime(TimeFrameIndex(1), std::move(mask), NotifyObservers::No);

        auto save_opts = defaultSaveOpts();
        save_opts.save_header = false;

        bool const save_ok = save(mask_data.get(), save_opts);
        REQUIRE(save_ok);

        auto load_opts = defaultLoadOpts();
        load_opts.has_header = false;

        auto loaded_map = load(load_opts);
        REQUIRE(loaded_map.size() == 1);
        REQUIRE(loaded_map[TimeFrameIndex(1)].size() == 1);
        CHECK(loaded_map[TimeFrameIndex(1)][0].size() == 2);
    }

    SECTION("CSV file contents are correct") {
        auto mask_data = std::make_shared<MaskData>();
        // Row y=5, pixels at x=10,11,12 -> RLE triplet: 5,10,3
        Mask2D mask({{10, 5}, {11, 5}, {12, 5}});
        mask_data->addAtTime(TimeFrameIndex(7), std::move(mask), NotifyObservers::No);

        bool const save_ok = save(mask_data.get(), defaultSaveOpts());
        REQUIRE(save_ok);

        std::ifstream file(csv_filepath);
        std::string line;

        REQUIRE(std::getline(file, line));
        REQUIRE(line == "Frame,RLE");

        REQUIRE(std::getline(file, line));
        REQUIRE(line == "7,\"5,10,3\"");

        file.close();
    }
}
