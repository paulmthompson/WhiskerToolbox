/**
 * @file hdf5_bitpacked_digital.test.cpp
 * @brief Tests for HDF5 bit-packed digital event/interval and identity TimeFrame loading
 */

#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "IO/core/LoaderRegistration.hpp"
#include "IO/core/LoaderRegistry.hpp"
#include "IO/formats/HDF5/HDF5Loader.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <H5Cpp.h>

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

namespace {

class BitPackedHDF5Fixture {
public:
    BitPackedHDF5Fixture() {
        registerAllLoaders();
        test_dir = std::filesystem::temp_directory_path() / "hdf5_bitpacked_test";
        std::filesystem::create_directories(test_dir);
        filepath = test_dir / "digital_scans.h5";
        createTestFile();
    }

    ~BitPackedHDF5Fixture() {
        std::error_code ec;
        std::filesystem::remove_all(test_dir, ec);
    }

    [[nodiscard]] std::string path() const { return filepath.string(); }

private:
    void createTestFile() {
        // 1D dataset: values [0, 1, 1, 0, 2, 2, 0]
        // ch0 rising edges at indices 1; ch1 rising at 4; ch1 interval [4, 6]
        std::vector<uint8_t> const samples_1d = {0, 1, 1, 0, 2, 2, 0};

        H5::H5File const file(filepath.string(), H5F_ACC_TRUNC);
        hsize_t const dims_1d[] = {samples_1d.size()};
        H5::DataSpace const space_1d(1, dims_1d);
        H5::DataSet const dataset_1d = file.createDataSet("digital_1d", H5::PredType::NATIVE_UINT8, space_1d);
        dataset_1d.write(samples_1d.data(), H5::PredType::NATIVE_UINT8);

        file.createGroup("sweep_0001");
        hsize_t const dims_2d[] = {1, samples_1d.size()};
        H5::DataSpace const space_2d(2, dims_2d);
        H5::DataSet const dataset_2d =
                file.createDataSet("sweep_0001/digitalScans", H5::PredType::NATIVE_UINT8, space_2d);
        dataset_2d.write(samples_1d.data(), H5::PredType::NATIVE_UINT8);
    }

    std::filesystem::path test_dir;
    std::filesystem::path filepath;
};

nlohmann::json baseConfig(std::string const & path, std::string const & data_key) {
    return nlohmann::json{
            {"data_key", data_key},
            {"channel", 0},
            {"transition", "rising"}};
}

}// namespace

TEST_CASE("HDF5Loader bit-packed digital events direct", "[hdf5][digital_event][bitpacked]") {
    BitPackedHDF5Fixture const fixture;

    HDF5Loader const loader;
    nlohmann::json const config = baseConfig(fixture.path(), "digital_1d");
    auto result = loader.loadData(fixture.path(), DM_DataType::DigitalEvent, config);

    INFO("Load error: " << result.error_message);
    REQUIRE(result.success);
    auto const events = std::get<std::shared_ptr<DigitalEventSeries>>(result.data);
    REQUIRE(events->size() == 1);
    REQUIRE(events->getStoredEvent(0).getValue() == 1);
}

TEST_CASE("HDF5 bit-packed digital events from 1D uint8 dataset", "[hdf5][digital_event][bitpacked]") {
    BitPackedHDF5Fixture const fixture;
    auto & registry = LoaderRegistry::getInstance();

    auto config = baseConfig(fixture.path(), "digital_1d");
    auto result = registry.tryLoad("hdf5", DM_DataType::DigitalEvent, fixture.path(), config);

    INFO("Load error: " << result.error_message);
    REQUIRE(result.success);
    REQUIRE(std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(result.data));

    auto const events = std::get<std::shared_ptr<DigitalEventSeries>>(result.data);
    REQUIRE(events != nullptr);
    REQUIRE(events->size() == 1);
    REQUIRE(events->getStoredEvent(0).getValue() == 1);
}

TEST_CASE("HDF5 bit-packed digital events from 2D Wavesurfer layout", "[hdf5][digital_event][bitpacked]") {
    BitPackedHDF5Fixture const fixture;
    auto & registry = LoaderRegistry::getInstance();

    auto config = baseConfig(fixture.path(), "sweep_0001/digitalScans");
    config["channel"] = 1;
    auto result = registry.tryLoad("hdf5", DM_DataType::DigitalEvent, fixture.path(), config);

    REQUIRE(result.success);
    auto const events = std::get<std::shared_ptr<DigitalEventSeries>>(result.data);
    REQUIRE(events->size() == 1);
    REQUIRE(events->getStoredEvent(0).getValue() == 4);
}

TEST_CASE("HDF5 bit-packed digital intervals from 1D uint8 dataset", "[hdf5][digital_interval][bitpacked]") {
    BitPackedHDF5Fixture const fixture;
    auto & registry = LoaderRegistry::getInstance();

    auto config = baseConfig(fixture.path(), "digital_1d");
    config["channel"] = 1;
    auto result = registry.tryLoad("hdf5", DM_DataType::DigitalInterval, fixture.path(), config);

    REQUIRE(result.success);
    REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result.data));

    auto const intervals = std::get<std::shared_ptr<DigitalIntervalSeries>>(result.data);
    REQUIRE(intervals != nullptr);
    REQUIRE(intervals->size() == 1);

    auto const interval = intervals->getStoredInterval(0);
    REQUIRE(interval.start.getValue() == 4);
    REQUIRE(interval.end.getValue() == 6);
}

TEST_CASE("HDF5 identity TimeFrame from dataset shape", "[hdf5][timeframe][identity]") {
    BitPackedHDF5Fixture const fixture;
    auto & registry = LoaderRegistry::getInstance();

    nlohmann::json const config = {
            {"data_key", "digital_1d"},
            {"time_layout", "identity"}};

    auto result = registry.tryLoad("hdf5", DM_DataType::Time, fixture.path(), config);

    REQUIRE(result.success);
    REQUIRE(std::holds_alternative<std::shared_ptr<TimeFrame>>(result.data));

    auto const timeframe = std::get<std::shared_ptr<TimeFrame>>(result.data);
    REQUIRE(timeframe != nullptr);
    REQUIRE(timeframe->getTotalFrameCount() == 7);
    REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex{0}).getValue() == 0);
    REQUIRE(timeframe->getTimeAtIndex(TimeFrameIndex{6}).getValue() == 6);
}

TEST_CASE("HDF5 bit-packed loader rejects missing data_key", "[hdf5][digital_event][bitpacked]") {
    BitPackedHDF5Fixture const fixture;
    auto & registry = LoaderRegistry::getInstance();

    nlohmann::json const config = {{"channel", 0}, {"transition", "rising"}};
    auto result = registry.tryLoad("hdf5", DM_DataType::DigitalInterval, fixture.path(), config);

    REQUIRE_FALSE(result.success);
}

TEST_CASE("HDF5FormatLoader supports digital interval and time types", "[hdf5][loader]") {
    auto & registry = LoaderRegistry::getInstance();

    REQUIRE(registry.isFormatSupported("hdf5", DM_DataType::DigitalInterval));
    REQUIRE(registry.isFormatSupported("hdf5", DM_DataType::Time));
}
