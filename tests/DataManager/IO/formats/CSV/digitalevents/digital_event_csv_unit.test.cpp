/**
 * @file digital_event_csv_unit.test.cpp
 * @brief Unit tests for DigitalEventSeries CSV save/load round-trip via direct function calls
 *
 * Tests include:
 * 1. Single event round-trip
 * 2. Multiple events round-trip
 * 3. Empty data (no events) save returns true
 * 4. Large event times round-trip
 * 5. Space-delimited round-trip
 */

#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "IO/formats/CSV/digitaltimeseries/Digital_Event_Series_CSV.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

class DigitalEventCSVUnitTestFixture {
public:
    DigitalEventCSVUnitTestFixture() {
        test_dir = std::filesystem::current_path() / "test_event_csv_unit_output";
        std::filesystem::create_directories(test_dir);
        csv_filename = "test_events.csv";
        csv_filepath = test_dir / csv_filename;
    }

    ~DigitalEventCSVUnitTestFixture() {
        cleanup();
    }

protected:
    void cleanup() {
        try {
            if (std::filesystem::exists(test_dir)) {
                std::filesystem::remove_all(test_dir);
            }
        } catch (std::exception const & e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }

    [[nodiscard]] CSVEventSaverOptions defaultSaveOpts() const {
        CSVEventSaverOptions opts;
        opts.filename = csv_filename;
        opts.parent_dir = test_dir.string();
        opts.save_header = true;
        opts.header = "Event";
        opts.precision = 3;
        opts.delimiter = ",";
        return opts;
    }

    [[nodiscard]] CSVEventLoaderOptions defaultLoadOpts() const {
        CSVEventLoaderOptions opts;
        opts.filepath = csv_filepath.string();
        opts.delimiter = ",";
        opts.has_header = true;
        opts.event_column = 0;
        opts.identifier_column = -1;
        opts.base_name = "test_events";
        return opts;
    }

    static void verifyEventEquality(DigitalEventSeries const & original,
                             DigitalEventSeries const & loaded) {
        REQUIRE(loaded.size() == original.size());

        auto orig_view = original.view();
        auto loaded_view = loaded.view();

        for (size_t i = 0; i < original.size(); ++i) {
            REQUIRE(loaded_view[i].time().getValue() == orig_view[i].time().getValue());
        }
    }

protected:
    std::filesystem::path test_dir;
    std::string csv_filename;
    std::filesystem::path csv_filepath;
};

TEST_CASE_METHOD(DigitalEventCSVUnitTestFixture,
                 "DM - IO - DigitalEventSeries - CSV save and load round-trip",
                 "[DigitalEventSeries][CSV][IO][unit]") {

    SECTION("Single event round-trip") {
        std::vector<TimeFrameIndex> const times = {TimeFrameIndex(42)};
        auto const original = std::make_shared<DigitalEventSeries>(times);

        bool const save_ok = save(original.get(), defaultSaveOpts());
        REQUIRE(save_ok);
        REQUIRE(std::filesystem::exists(csv_filepath));

        auto loaded_vec = load(defaultLoadOpts());
        REQUIRE(loaded_vec.size() == 1);
        REQUIRE(loaded_vec[0] != nullptr);

        verifyEventEquality(*original, *loaded_vec[0]);
    }

    SECTION("Multiple events round-trip") {
        std::vector<TimeFrameIndex> const times = {
                TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(100),
                TimeFrameIndex(500), TimeFrameIndex(1000), TimeFrameIndex(9999)};
        auto const original = std::make_shared<DigitalEventSeries>(times);

        bool const save_ok = save(original.get(), defaultSaveOpts());
        REQUIRE(save_ok);

        auto loaded_vec = load(defaultLoadOpts());
        REQUIRE(loaded_vec.size() == 1);
        verifyEventEquality(*original, *loaded_vec[0]);
    }

    SECTION("Empty event series") {
        std::vector<TimeFrameIndex> const times = {};
        auto const original = std::make_shared<DigitalEventSeries>(times);

        bool const save_ok = save(original.get(), defaultSaveOpts());
        REQUIRE(save_ok);
        REQUIRE(std::filesystem::exists(csv_filepath));

        // The loader returns empty vector when no events found
        auto loaded_vec = load(defaultLoadOpts());
        REQUIRE(loaded_vec.empty());
    }

    SECTION("Large event times round-trip") {
        std::vector<TimeFrameIndex> const times = {
                TimeFrameIndex(50000), TimeFrameIndex(100000),
                TimeFrameIndex(500000), TimeFrameIndex(999999)};
        auto const original = std::make_shared<DigitalEventSeries>(times);

        bool const save_ok = save(original.get(), defaultSaveOpts());
        REQUIRE(save_ok);

        auto loaded_vec = load(defaultLoadOpts());
        REQUIRE(loaded_vec.size() == 1);
        verifyEventEquality(*original, *loaded_vec[0]);
    }

    SECTION("Space-delimited round-trip") {
        std::vector<TimeFrameIndex> const times = {
                TimeFrameIndex(5), TimeFrameIndex(15), TimeFrameIndex(25)};
        auto const original = std::make_shared<DigitalEventSeries>(times);

        auto save_opts = defaultSaveOpts();
        save_opts.delimiter = " ";
        save_opts.save_header = false;

        bool const save_ok = save(original.get(), save_opts);
        REQUIRE(save_ok);

        auto load_opts = defaultLoadOpts();
        load_opts.delimiter = " ";
        load_opts.has_header = false;

        auto loaded_vec = load(load_opts);
        REQUIRE(loaded_vec.size() == 1);
        verifyEventEquality(*original, *loaded_vec[0]);
    }

    SECTION("CSV file contents are correct") {
        std::vector<TimeFrameIndex> const times = {
                TimeFrameIndex(10), TimeFrameIndex(20)};
        auto const original = std::make_shared<DigitalEventSeries>(times);

        bool const save_ok = save(original.get(), defaultSaveOpts());
        REQUIRE(save_ok);

        // Verify file contents
        std::ifstream file(csv_filepath);
        std::string line;

        REQUIRE(std::getline(file, line));
        REQUIRE(line == "Event");

        REQUIRE(std::getline(file, line));
        REQUIRE(line == "10");

        REQUIRE(std::getline(file, line));
        REQUIRE(line == "20");

        file.close();
    }
}
