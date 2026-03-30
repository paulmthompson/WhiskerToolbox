/**
 * @file tensor_npy_roundtrip.test.cpp
 * @brief Round-trip tests for TensorData Numpy (.npy) save and load
 *
 * Tests exercise creating a TensorData, saving it to .npy (with a companion
 * _rows.npy sidecar), loading it back, and verifying that shape and values
 * match. The .npy format preserves float values exactly (binary) but column
 * names are not preserved.
 */

#include <catch2/catch_test_macros.hpp>

#include "IO/formats/Numpy/tensordata/Tensor_Data_numpy.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TimeFrame/interval_data.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace {

class TensorNpyTestFixture {
public:
    TensorNpyTestFixture() {
        test_dir = std::filesystem::current_path() / "test_tensor_npy_output";
        std::filesystem::create_directories(test_dir);
    }

    ~TensorNpyTestFixture() {
        try {
            if (std::filesystem::exists(test_dir)) {
                std::filesystem::remove_all(test_dir);
            }
        } catch (...) {}
    }

protected:
    std::filesystem::path test_dir;
};

}// anonymous namespace

TEST_CASE_METHOD(TensorNpyTestFixture,
                 "TensorData npy round-trip preserves shape and values",
                 "[tensor][npy][roundtrip]") {
    std::vector<float> data = {
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f};

    auto original = TensorData::createOrdinal2D(data, 3, 4, {"a", "b", "c", "d"});

    // Save
    NpyTensorSaverOptions save_opts;
    save_opts.filename = "tensor_roundtrip.npy";
    save_opts.parent_dir = test_dir.string();

    REQUIRE(save(&original, save_opts));

    auto const npy_path = test_dir / "tensor_roundtrip.npy";
    auto const rows_path = test_dir / "tensor_roundtrip_rows.npy";
    REQUIRE(std::filesystem::exists(npy_path));
    REQUIRE(std::filesystem::exists(rows_path));

    // Load
    NpyTensorLoaderOptions load_opts;
    load_opts.filepath = npy_path.string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    // Verify shape
    REQUIRE(loaded->numRows() == 3);
    REQUIRE(loaded->numColumns() == 4);

    // Verify data values (binary exact — no float precision loss)
    for (std::size_t r = 0; r < 3; ++r) {
        auto row_data = loaded->row(r);
        REQUIRE(row_data.size() == 4);
        for (std::size_t c = 0; c < 4; ++c) {
            CHECK(row_data[c] == data[r * 4 + c]);
        }
    }

    // Ordinal sidecar → loaded as ordinal (sequential 0..N-1)
    CHECK(loaded->rowType() == RowType::Ordinal);
}

TEST_CASE_METHOD(TensorNpyTestFixture,
                 "TensorData npy round-trip with single column",
                 "[tensor][npy][roundtrip]") {
    std::vector<float> data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};

    auto original = TensorData::createOrdinal2D(data, 5, 1, {});

    NpyTensorSaverOptions save_opts;
    save_opts.filename = "single_col.npy";
    save_opts.parent_dir = test_dir.string();

    REQUIRE(save(&original, save_opts));

    NpyTensorLoaderOptions load_opts;
    load_opts.filepath = (test_dir / "single_col.npy").string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    REQUIRE(loaded->numRows() == 5);
    REQUIRE(loaded->numColumns() == 1);

    for (std::size_t r = 0; r < 5; ++r) {
        auto row_data = loaded->row(r);
        CHECK(row_data[0] == data[r]);
    }
}

TEST_CASE_METHOD(TensorNpyTestFixture,
                 "TensorData npy round-trip writes rows sidecar for ordinal",
                 "[tensor][npy][roundtrip]") {
    std::vector<float> data = {1.5f, 2.5f, 3.5f, 4.5f};
    auto original = TensorData::createOrdinal2D(data, 2, 2, {"x", "y"});

    NpyTensorSaverOptions save_opts;
    save_opts.filename = "ordinal.npy";
    save_opts.parent_dir = test_dir.string();

    REQUIRE(save(&original, save_opts));

    // Sidecar must exist
    REQUIRE(std::filesystem::exists(test_dir / "ordinal_rows.npy"));

    NpyTensorLoaderOptions load_opts;
    load_opts.filepath = (test_dir / "ordinal.npy").string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    REQUIRE(loaded->numRows() == 2);
    REQUIRE(loaded->numColumns() == 2);
    CHECK(loaded->rowType() == RowType::Ordinal);

    for (std::size_t r = 0; r < 2; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 2; ++c) {
            CHECK(row_data[c] == data[r * 2 + c]);
        }
    }
}

TEST_CASE_METHOD(TensorNpyTestFixture,
                 "TensorData npy load defaults to ordinal without sidecar",
                 "[tensor][npy][roundtrip]") {
    // Manually save just the data file (no sidecar) by saving then deleting it
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto original = TensorData::createOrdinal2D(data, 2, 2, {});

    NpyTensorSaverOptions save_opts;
    save_opts.filename = "no_sidecar.npy";
    save_opts.parent_dir = test_dir.string();

    REQUIRE(save(&original, save_opts));

    // Remove the sidecar
    auto const sidecar = test_dir / "no_sidecar_rows.npy";
    if (std::filesystem::exists(sidecar)) {
        std::filesystem::remove(sidecar);
    }

    NpyTensorLoaderOptions load_opts;
    load_opts.filepath = (test_dir / "no_sidecar.npy").string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    CHECK(loaded->numRows() == 2);
    CHECK(loaded->numColumns() == 2);
    CHECK(loaded->rowType() == RowType::Ordinal);
}

TEST_CASE("TensorData npy load throws on missing file",
          "[tensor][npy][error]") {
    NpyTensorLoaderOptions opts;
    opts.filepath = "/nonexistent/path/to/file.npy";

    REQUIRE_THROWS_AS(load(opts), std::runtime_error);
}

TEST_CASE_METHOD(TensorNpyTestFixture,
                 "TensorData npy round-trip preserves TimeFrameIndex rows",
                 "[tensor][npy][roundtrip]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    // Time indices: 10, 50, 100 (non-sequential → not ordinal)
    std::vector<TimeFrameIndex> time_indices = {
            TimeFrameIndex{10}, TimeFrameIndex{50}, TimeFrameIndex{100}};
    auto time_storage = TimeIndexStorageFactory::createFromTimeIndices(time_indices);

    auto original = TensorData::createTimeSeries2D(
            data, 3, 2, std::move(time_storage), nullptr, {"a", "b"});

    CHECK(original.rowType() == RowType::TimeFrameIndex);

    NpyTensorSaverOptions save_opts;
    save_opts.filename = "time_rows.npy";
    save_opts.parent_dir = test_dir.string();
    REQUIRE(save(&original, save_opts));

    NpyTensorLoaderOptions load_opts;
    load_opts.filepath = (test_dir / "time_rows.npy").string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    CHECK(loaded->numRows() == 3);
    CHECK(loaded->numColumns() == 2);
    CHECK(loaded->rowType() == RowType::TimeFrameIndex);
    CHECK(loaded->getTimeFrame() == nullptr);

    // Verify time indices round-tripped
    auto const & storage = loaded->rows().timeStorage();
    CHECK(storage.getTimeFrameIndexAt(0) == TimeFrameIndex{10});
    CHECK(storage.getTimeFrameIndexAt(1) == TimeFrameIndex{50});
    CHECK(storage.getTimeFrameIndexAt(2) == TimeFrameIndex{100});

    // Verify data
    for (std::size_t r = 0; r < 3; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 2; ++c) {
            CHECK(row_data[c] == data[r * 2 + c]);
        }
    }
}

TEST_CASE_METHOD(TensorNpyTestFixture,
                 "TensorData npy round-trip preserves Interval rows",
                 "[tensor][npy][roundtrip]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};

    std::vector<TimeFrameInterval> intervals = {
            {TimeFrameIndex{0}, TimeFrameIndex{100}},
            {TimeFrameIndex{200}, TimeFrameIndex{350}}};

    auto original = TensorData::createFromIntervals(
            data, 2, 2, intervals, nullptr, {"x", "y"});

    CHECK(original.rowType() == RowType::Interval);

    NpyTensorSaverOptions save_opts;
    save_opts.filename = "interval_rows.npy";
    save_opts.parent_dir = test_dir.string();
    REQUIRE(save(&original, save_opts));

    NpyTensorLoaderOptions load_opts;
    load_opts.filepath = (test_dir / "interval_rows.npy").string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    CHECK(loaded->numRows() == 2);
    CHECK(loaded->numColumns() == 2);
    CHECK(loaded->rowType() == RowType::Interval);
    CHECK(loaded->getTimeFrame() == nullptr);

    // Verify intervals round-tripped
    auto loaded_intervals = loaded->rows().intervals();
    REQUIRE(loaded_intervals.size() == 2);
    CHECK(loaded_intervals[0].start == TimeFrameIndex{0});
    CHECK(loaded_intervals[0].end == TimeFrameIndex{100});
    CHECK(loaded_intervals[1].start == TimeFrameIndex{200});
    CHECK(loaded_intervals[1].end == TimeFrameIndex{350});

    // Verify data
    for (std::size_t r = 0; r < 2; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 2; ++c) {
            CHECK(row_data[c] == data[r * 2 + c]);
        }
    }
}
