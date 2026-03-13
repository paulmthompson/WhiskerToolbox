/**
 * @file AnalogTimeSeriesInspector.test.cpp
 * @brief Unit tests for AnalogTimeSeriesInspector
 */

#include "AnalogTimeSeriesInspector.hpp"

#include "DataExport_Widget/AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "IO/core/LoaderRegistration.hpp"
#include "IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <numeric>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {
void ensureQApplication()
{
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());// NOLINT: Intentionally leaked
    }
}

struct RegistryInitializer {
    RegistryInitializer()
    {
        static bool initialized = false;
        if (!initialized) {
            registerInternalLoaders();
            initialized = true;
        }
    }
};

[[maybe_unused]] RegistryInitializer const g_registry_init{};

std::filesystem::path makeTempDir()
{
    auto dir = std::filesystem::temp_directory_path() / "whisker_analog_inspector_save_test";
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanupTempDir(std::filesystem::path const & dir)
{
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
}// namespace

// === AnalogTimeSeriesInspector Tests ===

TEST_CASE("AnalogTimeSeriesInspector construction", "[AnalogTimeSeriesInspector]")
{
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager")
    {
        auto data_manager = std::make_shared<DataManager>();
        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);

        app->processEvents();
    }

    SECTION("Constructs with nullptr group manager")
    {
        auto data_manager = std::make_shared<DataManager>();
        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.supportsGroupFiltering() == false);
        app->processEvents();
    }

    SECTION("Returns correct data type")
    {
        auto data_manager = std::make_shared<DataManager>();
        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.getDataType() == DM_DataType::Analog);
        REQUIRE(inspector.getTypeName() == QStringLiteral("Analog Time Series"));
        REQUIRE(inspector.supportsExport() == true);
    }
}

TEST_CASE("AnalogTimeSeriesInspector has expected UI", "[AnalogTimeSeriesInspector]")
{
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Contains filename edit")
    {
        auto data_manager = std::make_shared<DataManager>();
        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);

        auto * filename_edit = inspector.findChild<QLineEdit *>("filename_edit");
        REQUIRE(filename_edit != nullptr);

        app->processEvents();
    }

    SECTION("Contains export section")
    {
        auto data_manager = std::make_shared<DataManager>();
        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);

        auto * export_type_combo = inspector.findChild<QComboBox *>("export_type_combo");
        REQUIRE(export_type_combo != nullptr);
        REQUIRE(export_type_combo->count() > 0);

        app->processEvents();
    }

    SECTION("Contains CSV analog saver widget with save button")
    {
        auto data_manager = std::make_shared<DataManager>();
        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);

        auto * csv_saver = inspector.findChild<CSVAnalogSaver_Widget *>("csv_analog_saver_widget");
        REQUIRE(csv_saver != nullptr);

        auto * save_button = csv_saver->findChild<QPushButton *>("save_action_button");
        REQUIRE(save_button != nullptr);
        REQUIRE(save_button->text() == QStringLiteral("Save Analog to CSV"));

        app->processEvents();
    }
}

TEST_CASE("AnalogTimeSeriesInspector data display", "[AnalogTimeSeriesInspector]")
{
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Sets active key correctly")
    {
        auto data_manager = std::make_shared<DataManager>();

        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        std::vector<float> values = {1.5f, 2.3f, 3.7f, 4.1f, 5.9f};
        std::vector<TimeFrameIndex> times = {
            TimeFrameIndex(0), TimeFrameIndex(25), TimeFrameIndex(50), TimeFrameIndex(75), TimeFrameIndex(99)};
        auto analog_series = std::make_shared<AnalogTimeSeries>(values, times);
        data_manager->setData<AnalogTimeSeries>("test_analog", analog_series, TimeKey("time"));

        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_analog");

        app->processEvents();

        REQUIRE(inspector.getActiveKey() == "test_analog");
    }
}

TEST_CASE("AnalogTimeSeriesInspector saves data from DataManager", "[AnalogTimeSeriesInspector][save]")
{
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Save button exports AnalogTimeSeries to CSV")
    {
        auto temp_dir = makeTempDir();
        auto data_manager = std::make_shared<DataManager>();
        data_manager->setOutputPath(temp_dir.string());

        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        std::vector<float> values = {1.5f, 2.3f, 3.7f, 4.1f, 5.9f};
        std::vector<TimeFrameIndex> times = {
            TimeFrameIndex(0), TimeFrameIndex(25), TimeFrameIndex(50), TimeFrameIndex(75), TimeFrameIndex(99)};
        auto analog_series = std::make_shared<AnalogTimeSeries>(values, times);
        data_manager->setData<AnalogTimeSeries>("test_analog", analog_series, TimeKey("time"));

        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_analog");

        app->processEvents();

        auto * filename_edit = inspector.findChild<QLineEdit *>("filename_edit");
        REQUIRE(filename_edit != nullptr);
        filename_edit->setText(QStringLiteral("saved_analog.csv"));

        QTimer::singleShot(50, []() {
            if (auto * w = QApplication::activeModalWidget()) {
                w->close();
            }
        });

        auto * csv_saver = inspector.findChild<CSVAnalogSaver_Widget *>("csv_analog_saver_widget");
        REQUIRE(csv_saver != nullptr);
        auto * save_button = csv_saver->findChild<QPushButton *>("save_action_button");
        REQUIRE(save_button != nullptr);
        save_button->click();

        app->processEvents();

        auto const filepath = temp_dir / "saved_analog.csv";
        REQUIRE(std::filesystem::exists(filepath));

        CSVAnalogLoaderOptions load_opts;
        load_opts.filepath = filepath.string();
        load_opts.has_header = true;
        load_opts.single_column_format = false;
        load_opts.time_column = 0;
        load_opts.data_column = 1;

        auto loaded_series = load(load_opts);
        REQUIRE(loaded_series != nullptr);
        REQUIRE(loaded_series->getNumSamples() == 5);

        auto view = loaded_series->view();
        std::vector<std::pair<int64_t, float>> loaded_data;
        for (auto const & sample : view) {
            loaded_data.emplace_back(sample.time().getValue(), sample.value());
        }
        std::sort(loaded_data.begin(), loaded_data.end());

        REQUIRE(loaded_data[0].first == 0);
        REQUIRE_THAT(loaded_data[0].second, WithinAbs(1.5f, 0.01f));
        REQUIRE(loaded_data[1].first == 25);
        REQUIRE_THAT(loaded_data[1].second, WithinAbs(2.3f, 0.01f));
        REQUIRE(loaded_data[2].first == 50);
        REQUIRE_THAT(loaded_data[2].second, WithinAbs(3.7f, 0.01f));
        REQUIRE(loaded_data[3].first == 75);
        REQUIRE_THAT(loaded_data[3].second, WithinAbs(4.1f, 0.01f));
        REQUIRE(loaded_data[4].first == 99);
        REQUIRE_THAT(loaded_data[4].second, WithinAbs(5.9f, 0.01f));

        cleanupTempDir(temp_dir);
    }
}

TEST_CASE("AnalogTimeSeriesInspector callbacks", "[AnalogTimeSeriesInspector]")
{
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Removes callbacks on destruction")
    {
        auto data_manager = std::make_shared<DataManager>();

        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        auto analog_series = std::make_shared<AnalogTimeSeries>(std::vector<float>{1.0f}, std::vector<TimeFrameIndex>{TimeFrameIndex(0)});
        data_manager->setData<AnalogTimeSeries>("test_analog", analog_series, TimeKey("time"));

        {
            AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);
            inspector.setActiveKey("test_analog");
            app->processEvents();
        }

        app->processEvents();
    }

    SECTION("Removes callbacks explicitly")
    {
        auto data_manager = std::make_shared<DataManager>();

        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        auto analog_series = std::make_shared<AnalogTimeSeries>(std::vector<float>{1.0f}, std::vector<TimeFrameIndex>{TimeFrameIndex(0)});
        data_manager->setData<AnalogTimeSeries>("test_analog", analog_series, TimeKey("time"));

        AnalogTimeSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_analog");
        app->processEvents();

        inspector.removeCallbacks();

        app->processEvents();
    }
}
