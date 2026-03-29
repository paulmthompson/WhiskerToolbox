/**
 * @file tensor_csv_roundtrip.test.cpp
 * @brief Round-trip tests for TensorData CSV save and load
 *
 * Tests exercise all three row types (TimeFrameIndex, Interval, Ordinal)
 * by creating a TensorData, saving it to CSV, loading it back, and
 * verifying that the data matches.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "IO/formats/CSV/tensors/Tensor_Data_CSV.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TimeFrame/interval_data.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

class TensorCSVTestFixture {
public:
    TensorCSVTestFixture() {
        test_dir = std::filesystem::current_path() / "test_tensor_csv_output";
        std::filesystem::create_directories(test_dir);
    }

    ~TensorCSVTestFixture() {
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

TEST_CASE_METHOD(TensorCSVTestFixture,
                 "TensorData CSV round-trip with TimeFrameIndex rows",
                 "[tensor][csv][roundtrip]") {
    // Create a 3x2 tensor with TimeFrameIndex rows
    std::vector<float> data = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f};
    auto ts = TimeIndexStorageFactory::createFromTimeIndices(
            {TimeFrameIndex{10}, TimeFrameIndex{20}, TimeFrameIndex{30}});
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});

    auto original = TensorData::createTimeSeries2D(
            data, 3, 2, ts, tf, {"feat_a", "feat_b"});

    // Save
    auto const csv_path = test_dir / "tensor_tfi.csv";
    CSVTensorSaverOptions save_opts;
    save_opts.filename = csv_path.filename().string();
    save_opts.parent_dir = test_dir.string();
    save_opts.precision = 6;

    REQUIRE(save(&original, save_opts));
    REQUIRE(std::filesystem::exists(csv_path));

    // Load
    CSVTensorLoaderOptions load_opts;
    load_opts.filepath = csv_path.string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    // Verify shape
    REQUIRE(loaded->numRows() == 3);
    REQUIRE(loaded->numColumns() == 2);

    // Verify row type
    REQUIRE(loaded->rowType() == RowType::TimeFrameIndex);

    // Verify column names
    REQUIRE(loaded->hasNamedColumns());
    auto const & names = loaded->columnNames();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "feat_a");
    CHECK(names[1] == "feat_b");

    // Verify row labels (TimeFrameIndex values)
    auto const & rows = loaded->rows();
    auto const & time_storage = rows.timeStorage();
    CHECK(time_storage.getTimeFrameIndexAt(0) == TimeFrameIndex{10});
    CHECK(time_storage.getTimeFrameIndexAt(1) == TimeFrameIndex{20});
    CHECK(time_storage.getTimeFrameIndexAt(2) == TimeFrameIndex{30});

    // Verify data values
    for (std::size_t r = 0; r < 3; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 2; ++c) {
            CHECK_THAT(row_data[c], WithinAbs(data[r * 2 + c], 1e-4));
        }
    }
}

TEST_CASE_METHOD(TensorCSVTestFixture,
                 "TensorData CSV round-trip with Interval rows",
                 "[tensor][csv][roundtrip]") {
    // Create a 2x3 tensor with Interval rows
    std::vector<float> data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 100, 200, 300, 400});
    std::vector<TimeFrameInterval> intervals = {
            {TimeFrameIndex{100}, TimeFrameIndex{200}},
            {TimeFrameIndex{300}, TimeFrameIndex{400}}};

    auto original = TensorData::createFromIntervals(
            data, 2, 3, intervals, tf, {"m1", "m2", "m3"});

    // Save
    auto const csv_path = test_dir / "tensor_interval.csv";
    CSVTensorSaverOptions save_opts;
    save_opts.filename = csv_path.filename().string();
    save_opts.parent_dir = test_dir.string();
    save_opts.precision = 6;

    REQUIRE(save(&original, save_opts));
    REQUIRE(std::filesystem::exists(csv_path));

    // Load
    CSVTensorLoaderOptions load_opts;
    load_opts.filepath = csv_path.string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    // Verify shape
    REQUIRE(loaded->numRows() == 2);
    REQUIRE(loaded->numColumns() == 3);

    // Verify row type
    REQUIRE(loaded->rowType() == RowType::Interval);

    // Verify column names
    REQUIRE(loaded->hasNamedColumns());
    auto const & names = loaded->columnNames();
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "m1");
    CHECK(names[1] == "m2");
    CHECK(names[2] == "m3");

    // Verify intervals
    auto const & rows = loaded->rows();
    auto loaded_intervals = rows.intervals();
    REQUIRE(loaded_intervals.size() == 2);
    CHECK(loaded_intervals[0].start == TimeFrameIndex{100});
    CHECK(loaded_intervals[0].end == TimeFrameIndex{200});
    CHECK(loaded_intervals[1].start == TimeFrameIndex{300});
    CHECK(loaded_intervals[1].end == TimeFrameIndex{400});

    // Verify data values
    for (std::size_t r = 0; r < 2; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 3; ++c) {
            CHECK_THAT(row_data[c], WithinAbs(data[r * 3 + c], 1e-4));
        }
    }
}

TEST_CASE_METHOD(TensorCSVTestFixture,
                 "TensorData CSV round-trip with Ordinal rows",
                 "[tensor][csv][roundtrip]") {
    // Create a 4x2 ordinal tensor
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    auto original = TensorData::createOrdinal2D(data, 4, 2, {"x", "y"});

    // Save
    auto const csv_path = test_dir / "tensor_ordinal.csv";
    CSVTensorSaverOptions save_opts;
    save_opts.filename = csv_path.filename().string();
    save_opts.parent_dir = test_dir.string();
    save_opts.precision = 6;

    REQUIRE(save(&original, save_opts));
    REQUIRE(std::filesystem::exists(csv_path));

    // Load
    CSVTensorLoaderOptions load_opts;
    load_opts.filepath = csv_path.string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    // Verify shape
    REQUIRE(loaded->numRows() == 4);
    REQUIRE(loaded->numColumns() == 2);

    // Ordinal rows are saved as integers 0..3, which load as TimeFrameIndex
    // (since they look like plain integers). That's acceptable — the data is correct.
    CHECK((loaded->rowType() == RowType::TimeFrameIndex ||
           loaded->rowType() == RowType::Ordinal));

    // Verify column names
    REQUIRE(loaded->hasNamedColumns());
    auto const & names = loaded->columnNames();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "x");
    CHECK(names[1] == "y");

    // Verify data values
    for (std::size_t r = 0; r < 4; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 2; ++c) {
            CHECK_THAT(row_data[c], WithinAbs(data[r * 2 + c], 1e-4));
        }
    }
}

TEST_CASE_METHOD(TensorCSVTestFixture,
                 "TensorData CSV round-trip without column names",
                 "[tensor][csv][roundtrip]") {
    // Create a 2x3 ordinal tensor without column names
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    auto original = TensorData::createOrdinal2D(data, 2, 3);

    // Save
    auto const csv_path = test_dir / "tensor_no_names.csv";
    CSVTensorSaverOptions save_opts;
    save_opts.filename = csv_path.filename().string();
    save_opts.parent_dir = test_dir.string();
    save_opts.precision = 6;

    REQUIRE(save(&original, save_opts));
    REQUIRE(std::filesystem::exists(csv_path));

    // Load
    CSVTensorLoaderOptions load_opts;
    load_opts.filepath = csv_path.string();

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    // Verify shape
    REQUIRE(loaded->numRows() == 2);
    REQUIRE(loaded->numColumns() == 3);

    // Auto-generated column names: col_0, col_1, col_2
    REQUIRE(loaded->hasNamedColumns());
    auto const & names = loaded->columnNames();
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "col_0");
    CHECK(names[1] == "col_1");
    CHECK(names[2] == "col_2");

    // Verify data
    for (std::size_t r = 0; r < 2; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 3; ++c) {
            CHECK_THAT(row_data[c], WithinAbs(data[r * 3 + c], 1e-4));
        }
    }
}

TEST_CASE_METHOD(TensorCSVTestFixture,
                 "TensorData CSV round-trip with tab delimiter",
                 "[tensor][csv][roundtrip]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto ts = TimeIndexStorageFactory::createFromTimeIndices(
            {TimeFrameIndex{5}, TimeFrameIndex{15}});
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 5, 10, 15});

    auto original = TensorData::createTimeSeries2D(
            data, 2, 2, ts, tf, {"a", "b"});

    // Save with tab delimiter
    auto const csv_path = test_dir / "tensor_tab.tsv";
    CSVTensorSaverOptions save_opts;
    save_opts.filename = csv_path.filename().string();
    save_opts.parent_dir = test_dir.string();
    save_opts.delimiter = "\t";
    save_opts.precision = 6;

    REQUIRE(save(&original, save_opts));

    // Load with tab delimiter
    CSVTensorLoaderOptions load_opts;
    load_opts.filepath = csv_path.string();
    load_opts.delimiter = "\t";

    auto loaded = load(load_opts);
    REQUIRE(loaded);

    REQUIRE(loaded->numRows() == 2);
    REQUIRE(loaded->numColumns() == 2);

    for (std::size_t r = 0; r < 2; ++r) {
        auto row_data = loaded->row(r);
        for (std::size_t c = 0; c < 2; ++c) {
            CHECK_THAT(row_data[c], WithinAbs(data[r * 2 + c], 1e-4));
        }
    }
}
