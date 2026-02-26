#include <benchmark/benchmark.h>
#include <vector>
#include <fstream>
#include <random>
#include <filesystem>
#include <algorithm>
#include <iostream>

#include "DataManager/AnalogTimeSeries/IO/JSON/Analog_Time_Series_JSON.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

class FileFixture : public benchmark::Fixture {
public:
    std::string filename;
    // 32 Million samples * 2 bytes = 64MB
    const size_t num_samples = 32 * 1024 * 1024;

    void SetUp(const benchmark::State& state) override {
        filename = "temp_benchmark_data.bin";

        // Generate file if it doesn't exist
        if (!fs::exists(filename)) {
             std::vector<int16_t> data(num_samples);
             std::mt19937 gen(42);
             std::uniform_int_distribution<int16_t> dis(-1000, 1000);
             // Fill with random data
             // Using std::generate is faster than loop
             std::generate(data.begin(), data.end(), [&]() { return dis(gen); });

             std::ofstream out(filename, std::ios::binary);
             out.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(int16_t));
        }
    }

    void TearDown(const benchmark::State& state) override {
        // Cleanup only after all threads are done
        if (state.thread_index() == 0) {
             if (fs::exists(filename)) {
                 fs::remove(filename);
             }
        }
    }
};

BENCHMARK_DEFINE_F(FileFixture, BM_LoadUint8)(benchmark::State& state) {
    for (auto _ : state) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            state.SkipWithError("Could not open file");
            return;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            benchmark::DoNotOptimize(buffer);
        }
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_samples * sizeof(int16_t)));
}
BENCHMARK_REGISTER_F(FileFixture, BM_LoadUint8)->Unit(benchmark::kMillisecond);

BENCHMARK_DEFINE_F(FileFixture, BM_LoadInt16)(benchmark::State& state) {
    for (auto _ : state) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            state.SkipWithError("Could not open file");
            return;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<int16_t> buffer(size / sizeof(int16_t));
        if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            benchmark::DoNotOptimize(buffer);
        }
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_samples * sizeof(int16_t)));
}
BENCHMARK_REGISTER_F(FileFixture, BM_LoadInt16)->Unit(benchmark::kMillisecond);

BENCHMARK_DEFINE_F(FileFixture, BM_LoadInt16ToFloat_ElementWise)(benchmark::State& state) {
    for (auto _ : state) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // First read as int16
        std::vector<int16_t> raw_data(size / sizeof(int16_t));
        file.read(reinterpret_cast<char*>(raw_data.data()), size);

        // Convert to float loop
        std::vector<float> float_data;
        float_data.reserve(raw_data.size());
        for (int16_t val : raw_data) {
            float_data.push_back(static_cast<float>(val));
        }
        benchmark::DoNotOptimize(float_data);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_samples * sizeof(int16_t)));
}
BENCHMARK_REGISTER_F(FileFixture, BM_LoadInt16ToFloat_ElementWise)->Unit(benchmark::kMillisecond);

BENCHMARK_DEFINE_F(FileFixture, BM_LoadInt16ToFloat_Chunked)(benchmark::State& state) {
    const size_t CHUNK_SIZE = 8192; // 8KB buffer (4096 int16s)
    for (auto _ : state) {
        std::ifstream file(filename, std::ios::binary);
        std::vector<float> float_data;
        float_data.reserve(num_samples);

        std::vector<int16_t> buffer(CHUNK_SIZE);
        while (file) {
            file.read(reinterpret_cast<char*>(buffer.data()), CHUNK_SIZE * sizeof(int16_t));
            std::streamsize count = file.gcount() / sizeof(int16_t);
            if (count == 0) break;

            for (std::streamsize i = 0; i < count; ++i) {
                float_data.push_back(static_cast<float>(buffer[i]));
            }
        }
        benchmark::DoNotOptimize(float_data);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_samples * sizeof(int16_t)));
}
BENCHMARK_REGISTER_F(FileFixture, BM_LoadInt16ToFloat_Chunked)->Unit(benchmark::kMillisecond);

BENCHMARK_DEFINE_F(FileFixture, BM_LoadInt16ToFloat_Transform)(benchmark::State& state) {
    for (auto _ : state) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<int16_t> raw_data(size / sizeof(int16_t));
        file.read(reinterpret_cast<char*>(raw_data.data()), size);

        std::vector<float> float_data(raw_data.size());
        std::transform(raw_data.begin(), raw_data.end(), float_data.begin(),
            [](int16_t val) { return static_cast<float>(val); });

        benchmark::DoNotOptimize(float_data);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_samples * sizeof(int16_t)));
}
BENCHMARK_REGISTER_F(FileFixture, BM_LoadInt16ToFloat_Transform)->Unit(benchmark::kMillisecond);

BENCHMARK_DEFINE_F(FileFixture, BM_AnalogTimeSeries_JSON)(benchmark::State& state) {
    nlohmann::json config = {
        {"filepath", filename},
        {"format", "binary"},
        {"binary_data_type", "int16"},
        {"num_channels", 1},
        {"header_size", 0},
        {"offset", 0},
        {"stride", 1},
        {"scale_factor", 1.0},
        {"offset_value", 0.0},
        {"num_samples", num_samples}
    };

    for (auto _ : state) {
        auto result = load_into_AnalogTimeSeries(filename, config);
        benchmark::DoNotOptimize(result);
        if (result.empty()) {
             state.SkipWithError("Result is empty!");
        }
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(num_samples * sizeof(int16_t)));
}
BENCHMARK_REGISTER_F(FileFixture, BM_AnalogTimeSeries_JSON)->Unit(benchmark::kMillisecond);
