#include <benchmark/benchmark.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>
#include <iostream>

#include "AnalogTimeSeries/IO/JSON/Analog_Time_Series_JSON.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

namespace {

void GenerateBinaryFile(const std::string& filename, size_t size_bytes) {
    std::ofstream file(filename, std::ios::binary);
    std::vector<uint8_t> data(size_bytes);
    // Simple pattern to avoid overhead of RNG, we just need bytes
    for (size_t i = 0; i < size_bytes; ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(size_bytes));
}

} // namespace

class AnalogLoadingBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        // Create a temporary file
        filename = "temp_benchmark_data.bin";
        // Ensure clean state
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        }
    }

    void TearDown(const benchmark::State& state) override {
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        }
    }

    std::string filename;
};

BENCHMARK_DEFINE_F(AnalogLoadingBenchmark, BestCase_RawBytes)(benchmark::State& state) {
    size_t num_bytes = static_cast<size_t>(state.range(0));
    GenerateBinaryFile(filename, num_bytes);

    for (auto _ : state) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Failed to open file");

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
             throw std::runtime_error("Failed to read file");
        }

        benchmark::DoNotOptimize(buffer.data());
    }
    state.SetBytesProcessed(state.iterations() * num_bytes);
}
BENCHMARK_REGISTER_F(AnalogLoadingBenchmark, BestCase_RawBytes)->Arg(1024 * 1024 * 10); // 10MB

BENCHMARK_DEFINE_F(AnalogLoadingBenchmark, AnalogTimeSeries_SingleChannel)(benchmark::State& state) {
    size_t num_bytes = static_cast<size_t>(state.range(0));
    GenerateBinaryFile(filename, num_bytes);

    // Config for single channel int16
    nlohmann::json config;
    config["format"] = "binary";
    config["binary_data_type"] = "int16";
    // num_samples is informative here if we pass it, but calculated from file size in loader
    config["num_samples"] = num_bytes / 2; // int16 = 2 bytes
    config["num_channels"] = 1;
    config["filepath"] = filename;

    for (auto _ : state) {
        auto result = load_into_AnalogTimeSeries(filename, config);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * num_bytes);
}
BENCHMARK_REGISTER_F(AnalogLoadingBenchmark, AnalogTimeSeries_SingleChannel)->Arg(1024 * 1024 * 10);

BENCHMARK_DEFINE_F(AnalogLoadingBenchmark, AnalogTimeSeries_MultiChannel)(benchmark::State& state) {
    size_t num_bytes = static_cast<size_t>(state.range(0));
    GenerateBinaryFile(filename, num_bytes);

    int num_channels = 32;

    // Config for multi channel int16
    nlohmann::json config;
    config["format"] = "binary";
    config["binary_data_type"] = "int16";
    config["num_samples"] = (num_bytes / 2) / num_channels;
    config["num_channels"] = num_channels;
    config["filepath"] = filename;

    for (auto _ : state) {
        auto result = load_into_AnalogTimeSeries(filename, config);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * num_bytes);
}
BENCHMARK_REGISTER_F(AnalogLoadingBenchmark, AnalogTimeSeries_MultiChannel)->Arg(1024 * 1024 * 10);

BENCHMARK_MAIN();
