/**
 * @file analog_binary_tensor_backed.test.cpp
 * @brief Tests for tensor-backed binary loading (Phase 1 of multi-channel roadmap)
 *
 * Tests the following:
 * 1. Tensor-backed multi-channel loading produces correct TensorData + views
 * 2. Zero-copy verification (pointer comparison)
 * 3. Values match legacy independent-channel path
 * 4. Data type dispatch fix (float32 in-memory multi-channel)
 * 5. JSON round-trip for use_tensor_backed option
 * 6. Fallback when use_tensor_backed is false or single channel
 */

#include <catch2/catch_test_macros.hpp>

#include "fixtures/scenarios/analog/binary_loading_scenarios.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "IO/formats/Binary/analogtimeseries/Analog_Time_Series_Binary.hpp"
#include "Tensors/TensorData.hpp"

#include <nlohmann/json.hpp>
#include <rfl/json.hpp>

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;

class TempTensorBackedTestDirectory {
public:
    TempTensorBackedTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() /
                    ("whiskertoolbox_tensor_backed_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }

    ~TempTensorBackedTestDirectory() {
        if (std::filesystem::exists(temp_path)) {
            std::filesystem::remove_all(temp_path);
        }
    }

    [[nodiscard]] std::filesystem::path getPath() const { return temp_path; }
    [[nodiscard]] std::string getPathString() const { return temp_path.string(); }
    [[nodiscard]] std::filesystem::path getFilePath(std::string const & filename) const {
        return temp_path / filename;
    }

private:
    std::filesystem::path temp_path;
};

//=============================================================================
// Helper: write multi-channel float32 interleaved binary
//=============================================================================
namespace {

bool writeBinaryFloat32MultiChannel(
        std::vector<std::shared_ptr<AnalogTimeSeries>> const & signals,
        std::string const & filepath) {

    if (signals.empty()) {
        return false;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    size_t const num_samples = signals[0]->getNumSamples();
    for (auto const & sig: signals) {
        if (sig->getNumSamples() != num_samples) {
            return false;
        }
    }

    // Write interleaved float32 data
    for (size_t i = 0; i < num_samples; ++i) {
        for (auto const & sig: signals) {
            auto samples = sig->getAllSamples();
            float value = samples[i].value();
            file.write(reinterpret_cast<char const *>(&value), sizeof(float));
        }
    }

    return file.good();
}

}// namespace

//=============================================================================
// Test: loadTensorBacked basic functionality
//=============================================================================

TEST_CASE("Tensor-backed binary loading - basic multi-channel",
          "[analog][binary][tensor_backed]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::two_channel_ramps();
    auto binary_path = temp_dir.getFilePath("two_channel.bin");
    REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 2;
    opts.use_tensor_backed = true;

    auto result = loadTensorBacked(opts);

    SECTION("TensorData is created") {
        REQUIRE(result.tensor != nullptr);
        REQUIRE(result.tensor->ndim() == 2);
        REQUIRE(result.tensor->numColumns() == 2);
        REQUIRE(result.tensor->numRows() == signals[0]->getNumSamples());
    }

    SECTION("Per-channel views are created") {
        REQUIRE(result.channels.size() == 2);
        REQUIRE(result.channels[0] != nullptr);
        REQUIRE(result.channels[1] != nullptr);
        REQUIRE(result.channels[0]->getNumSamples() == signals[0]->getNumSamples());
        REQUIRE(result.channels[1]->getNumSamples() == signals[1]->getNumSamples());
    }

    SECTION("Channel values match original data") {
        auto ch0_samples = result.channels[0]->getAllSamples();
        auto ch1_samples = result.channels[1]->getAllSamples();

        auto orig0_samples = signals[0]->getAllSamples();
        auto orig1_samples = signals[1]->getAllSamples();

        // int16 round-trip: values should match as int16_t truncation
        for (size_t i = 0; i < signals[0]->getNumSamples(); ++i) {
            REQUIRE(ch0_samples[i].value() ==
                    static_cast<float>(static_cast<int16_t>(orig0_samples[i].value())));
            REQUIRE(ch1_samples[i].value() ==
                    static_cast<float>(static_cast<int16_t>(orig1_samples[i].value())));
        }
    }
}

//=============================================================================
// Test: Zero-copy verification (pointer comparison)
//=============================================================================

TEST_CASE("Tensor-backed binary loading - zero-copy verification",
          "[analog][binary][tensor_backed]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::four_channel_constants();
    auto binary_path = temp_dir.getFilePath("four_channel.bin");
    REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 4;
    opts.use_tensor_backed = true;

    auto result = loadTensorBacked(opts);
    REQUIRE(result.tensor != nullptr);
    REQUIRE(result.channels.size() == 4);

    // Verify each channel's data pointer points into the TensorData's flat storage
    // (We compare against flatData() rather than using tryGetAs to avoid
    //  cross-shared-library RTTI issues with type-erased StorageModel.)
    auto flat = result.tensor->storage().flatData();
    REQUIRE(!flat.empty());

    auto const num_samples = signals[0]->getNumSamples();

    for (size_t ch = 0; ch < 4; ++ch) {
        auto span = result.channels[ch]->getAnalogTimeSeries();
        REQUIRE(!span.empty());
        // Column ch in a column-major matrix starts at offset ch * num_rows
        REQUIRE(span.data() == flat.data() + ch * num_samples);
    }
}

//=============================================================================
// Test: Values match legacy path
//=============================================================================

TEST_CASE("Tensor-backed binary loading - values match legacy path",
          "[analog][binary][tensor_backed]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::four_channel_constants();
    auto binary_path = temp_dir.getFilePath("four_channel_compare.bin");
    REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 4;

    // Legacy path
    opts.use_tensor_backed = false;
    auto legacy = loadTensorBacked(opts);
    REQUIRE(legacy.tensor == nullptr);
    REQUIRE(legacy.channels.size() == 4);

    // Tensor-backed path
    opts.use_tensor_backed = true;
    auto tensor_backed = loadTensorBacked(opts);
    REQUIRE(tensor_backed.tensor != nullptr);
    REQUIRE(tensor_backed.channels.size() == 4);

    // Compare all values
    for (size_t ch = 0; ch < 4; ++ch) {
        auto legacy_samples = legacy.channels[ch]->getAllSamples();
        auto tensor_samples = tensor_backed.channels[ch]->getAllSamples();

        REQUIRE(legacy_samples.size() == tensor_samples.size());
        for (size_t i = 0; i < legacy_samples.size(); ++i) {
            REQUIRE(legacy_samples[i].value() == tensor_samples[i].value());
        }
    }
}

//=============================================================================
// Test: Data type dispatch fix - float32 in-memory multi-channel
//=============================================================================

TEST_CASE("Tensor-backed binary loading - float32 data type dispatch",
          "[analog][binary][tensor_backed]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::two_channel_ramps();
    auto binary_path = temp_dir.getFilePath("float32_multichannel.bin");
    REQUIRE(writeBinaryFloat32MultiChannel(signals, binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 2;
    opts.binary_data_type = "float32";
    opts.use_tensor_backed = true;

    auto result = loadTensorBacked(opts);
    REQUIRE(result.tensor != nullptr);
    REQUIRE(result.channels.size() == 2);

    // Float32 should preserve exact values (no int16 truncation)
    auto ch0_samples = result.channels[0]->getAllSamples();
    auto orig0_samples = signals[0]->getAllSamples();

    for (size_t i = 0; i < signals[0]->getNumSamples(); ++i) {
        REQUIRE(ch0_samples[i].value() == orig0_samples[i].value());
    }
}

//=============================================================================
// Test: float32 in-memory multi-channel via legacy load() (data type fix)
//=============================================================================

TEST_CASE("Binary data type dispatch fix - float32 multi-channel in-memory",
          "[analog][binary][data_type_fix]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::two_channel_ramps();
    auto binary_path = temp_dir.getFilePath("float32_legacy.bin");
    REQUIRE(writeBinaryFloat32MultiChannel(signals, binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 2;
    opts.binary_data_type = "float32";
    // use_tensor_backed = false (default)

    auto channels = load(opts);
    REQUIRE(channels.size() == 2);

    // Float32 should preserve exact values
    auto ch0_samples = channels[0]->getAllSamples();
    auto orig0_samples = signals[0]->getAllSamples();

    for (size_t i = 0; i < signals[0]->getNumSamples(); ++i) {
        REQUIRE(ch0_samples[i].value() == orig0_samples[i].value());
    }
}

//=============================================================================
// Test: Fallback - single channel with tensor_backed true
//=============================================================================

TEST_CASE("Tensor-backed binary loading - single channel fallback",
          "[analog][binary][tensor_backed]") {

    TempTensorBackedTestDirectory temp_dir;

    auto original = analog_scenarios::simple_ramp_100();
    auto binary_path = temp_dir.getFilePath("single_channel.bin");
    REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 1;
    opts.use_tensor_backed = true;// Should be ignored for single channel

    auto result = loadTensorBacked(opts);

    // Single channel → no tensor, just the channel
    REQUIRE(result.tensor == nullptr);
    REQUIRE(result.channels.size() == 1);
    REQUIRE(result.channels[0]->getNumSamples() == original->getNumSamples());
}

//=============================================================================
// Test: Fallback - memory-mapped with tensor_backed true
//=============================================================================

TEST_CASE("Tensor-backed binary loading - memory-mapped fallback",
          "[analog][binary][tensor_backed]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::two_channel_ramps();
    auto binary_path = temp_dir.getFilePath("mmap_fallback.bin");
    REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 2;
    opts.use_memory_mapped = true;
    opts.use_tensor_backed = true;// Should be ignored when mmap is on
    opts.binary_data_type = "int16";

    auto result = loadTensorBacked(opts);

    // Mmap → no tensor, just legacy channels
    REQUIRE(result.tensor == nullptr);
    REQUIRE(result.channels.size() == 2);
}

//=============================================================================
// Test: JSON config loading via DataManager
//=============================================================================

TEST_CASE("Tensor-backed binary loading - JSON config integration",
          "[analog][binary][tensor_backed][datamanager]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::four_channel_constants();
    auto binary_path = temp_dir.getFilePath("json_tensor_backed.bin");
    REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));

    json config = json::array({{{"data_type", "analog"},
                                {"name", "tensor_test"},
                                {"filepath", binary_path.string()},
                                {"format", "binary"},
                                {"num_channels", 4},
                                {"header_size", 0},
                                {"use_tensor_backed", true}}});

    DataManager dm;
    load_data_from_json_config(&dm, config, temp_dir.getPathString());

    // TensorData should be registered under the base name
    auto tensor = dm.getData<TensorData>("tensor_test");
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->numColumns() == 4);
    REQUIRE(tensor->numRows() == 50);

    // Per-channel AnalogTimeSeries should be registered as tensor_test_0, ..., tensor_test_3
    for (int ch = 0; ch < 4; ++ch) {
        std::string key = "tensor_test_" + std::to_string(ch);
        auto loaded = dm.getData<AnalogTimeSeries>(key);
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == 50);

        float expected_value = 10.0f * (ch + 1);// 10, 20, 30, 40
        auto samples = loaded->getAllSamples();
        for (auto const & sample: samples) {
            REQUIRE(sample.value() == expected_value);
        }
    }
}

//=============================================================================
// Test: TensorData column names match channel indices
//=============================================================================

TEST_CASE("Tensor-backed binary loading - column names",
          "[analog][binary][tensor_backed]") {

    TempTensorBackedTestDirectory temp_dir;

    auto signals = analog_scenarios::two_channel_ramps();
    auto binary_path = temp_dir.getFilePath("col_names.bin");
    REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));

    BinaryAnalogLoaderOptions opts;
    opts.filepath = binary_path.string();
    opts.num_channels = 2;
    opts.use_tensor_backed = true;

    auto result = loadTensorBacked(opts);
    REQUIRE(result.tensor != nullptr);
    REQUIRE(result.tensor->hasNamedColumns());

    auto const & names = result.tensor->columnNames();
    REQUIRE(names.size() == 2);
    REQUIRE(names[0] == "0");
    REQUIRE(names[1] == "1");
}

//=============================================================================
// Test: JSON round-trip for BinaryAnalogLoaderOptions with use_tensor_backed
//=============================================================================

TEST_CASE("BinaryAnalogLoaderOptions - use_tensor_backed JSON round-trip",
          "[analog][binary][tensor_backed][reflection]") {

    BinaryAnalogLoaderOptions opts;
    opts.filepath = "test.bin";
    opts.num_channels = 4;
    opts.use_tensor_backed = true;

    auto json_str = rfl::json::write(opts);
    auto parsed = rfl::json::read<BinaryAnalogLoaderOptions>(json_str);

    REQUIRE(parsed);
    REQUIRE(parsed.value().getUseTensorBacked() == true);
    REQUIRE(parsed.value().getNumChannels() == 4);
}
