/**
 * @file ScatterPlotPropertiesWidget.test.cpp
 * @brief Unit tests for ScatterPlotPropertiesWidget
 *
 * Tests the properties widget for ScatterPlotWidget, including:
 * - Combo box population with AnalogTimeSeries and TensorData keys
 * - DataManager observer callback functionality
 * - Combo box refresh when data is added/removed
 * - Stale key handling when data is deleted
 * - State synchronization
 */

#include "UI/ScatterPlotPropertiesWidget.hpp"
#include "Core/ScatterPlotState.hpp"
#include "Core/ScatterAxisSource.hpp"

#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include <QComboBox>

#include <memory>
#include <string>
#include <vector>

// =============================================================================
// Helpers
// =============================================================================

namespace {

std::shared_ptr<TimeFrame> createTestTimeFrame()
{
    std::vector<int> times;
    times.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

std::shared_ptr<DataManager> createDataManager()
{
    auto dm = std::make_shared<DataManager>();
    auto tf = createTestTimeFrame();
    dm->setTime(TimeKey("time"), tf);
    return dm;
}

void addAnalogTimeSeries(DataManager & dm, std::string const & key)
{
    std::vector<float> values(100, 1.0f);
    auto ats = std::make_shared<AnalogTimeSeries>(values, values.size());
    ats->setTimeFrame(dm.getTime(TimeKey("time")));
    dm.setData<AnalogTimeSeries>(key, ats, TimeKey("time"));
}

void addTensorData(DataManager & dm, std::string const & key)
{
    std::vector<float> data(100, 2.0f);
    auto ts = TimeIndexStorageFactory::createDenseFromZero(100);
    auto tf = dm.getTime(TimeKey("time"));
    auto tensor = std::make_shared<TensorData>(
        TensorData::createTimeSeries2D(data, 100, 1, ts, tf, {"col_0"}));
    dm.setData<TensorData>(key, tensor, TimeKey("time"));
}

/// Find X key combo box
QComboBox * findXKeyCombo(ScatterPlotPropertiesWidget * widget)
{
    // The x_key_combo is a private member, but we can find it via objectName
    // or by traversing children. We need to ensure it's findable.
    // The widget creates _x_key_combo without setting objectName, so we search
    // by class type and order.
    auto combos = widget->findChildren<QComboBox *>();
    // X key combo is the first one added
    return combos.size() >= 1 ? combos[0] : nullptr;
}

/// Find Y key combo box
QComboBox * findYKeyCombo(ScatterPlotPropertiesWidget * widget)
{
    auto combos = widget->findChildren<QComboBox *>();
    // Y key combo is the third (after X key and X column)
    return combos.size() >= 3 ? combos[2] : nullptr;
}

/// Check if combo contains a key (with either ATS or Tensor prefix)
bool comboContainsKey(QComboBox * combo, std::string const & key)
{
    if (!combo) {
        return false;
    }
    QString ats_text = QString::fromStdString("[ATS] " + key);
    QString td_text = QString::fromStdString("[Tensor] " + key);
    return combo->findText(ats_text) >= 0 || combo->findText(td_text) >= 0;
}

}  // namespace

// =============================================================================
// Tests
// =============================================================================

TEST_CASE("ScatterPlotPropertiesWidget combo box population", "[ScatterPlotPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("empty combo boxes when no data available")
    {
        auto dm = createDataManager();
        auto state = std::make_shared<ScatterPlotState>();

        ScatterPlotPropertiesWidget widget(state, dm);

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(x_combo != nullptr);
        // Should only have the empty placeholder
        REQUIRE(x_combo->count() == 1);
        REQUIRE(x_combo->itemText(0).isEmpty());
    }

    SECTION("combo boxes populated with AnalogTimeSeries keys")
    {
        auto dm = createDataManager();
        addAnalogTimeSeries(*dm, "signal_1");
        addAnalogTimeSeries(*dm, "signal_2");

        auto state = std::make_shared<ScatterPlotState>();
        ScatterPlotPropertiesWidget widget(state, dm);

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(x_combo != nullptr);
        // Empty placeholder + 2 ATS keys
        REQUIRE(x_combo->count() == 3);
        REQUIRE(comboContainsKey(x_combo, "signal_1"));
        REQUIRE(comboContainsKey(x_combo, "signal_2"));
    }

    SECTION("combo boxes populated with TensorData keys")
    {
        auto dm = createDataManager();
        addTensorData(*dm, "tensor_1");
        addTensorData(*dm, "tensor_2");

        auto state = std::make_shared<ScatterPlotState>();
        ScatterPlotPropertiesWidget widget(state, dm);

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(x_combo != nullptr);
        // Empty placeholder + 2 Tensor keys
        REQUIRE(x_combo->count() == 3);
        REQUIRE(comboContainsKey(x_combo, "tensor_1"));
        REQUIRE(comboContainsKey(x_combo, "tensor_2"));
    }

    SECTION("combo boxes populated with mixed ATS and TensorData keys")
    {
        auto dm = createDataManager();
        addAnalogTimeSeries(*dm, "analog");
        addTensorData(*dm, "tensor");

        auto state = std::make_shared<ScatterPlotState>();
        ScatterPlotPropertiesWidget widget(state, dm);

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(x_combo != nullptr);
        // Empty placeholder + 1 ATS + 1 Tensor
        REQUIRE(x_combo->count() == 3);
        REQUIRE(comboContainsKey(x_combo, "analog"));
        REQUIRE(comboContainsKey(x_combo, "tensor"));
    }
}

TEST_CASE("ScatterPlotPropertiesWidget DataManager observer", "[ScatterPlotPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("observer refresh on data add")
    {
        auto dm = createDataManager();
        auto state = std::make_shared<ScatterPlotState>();

        ScatterPlotPropertiesWidget widget(state, dm);

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(x_combo != nullptr);
        int initial_count = x_combo->count();

        // Add data — observer should trigger repopulation
        addAnalogTimeSeries(*dm, "new_signal");
        QApplication::processEvents();

        REQUIRE(x_combo->count() > initial_count);
        REQUIRE(comboContainsKey(x_combo, "new_signal"));
    }

    SECTION("observer refresh on data remove")
    {
        auto dm = createDataManager();
        addAnalogTimeSeries(*dm, "to_remove");
        addAnalogTimeSeries(*dm, "to_keep");

        auto state = std::make_shared<ScatterPlotState>();
        ScatterPlotPropertiesWidget widget(state, dm);

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(comboContainsKey(x_combo, "to_remove"));
        REQUIRE(comboContainsKey(x_combo, "to_keep"));

        // Remove data
        dm->deleteData("to_remove");
        QApplication::processEvents();

        REQUIRE_FALSE(comboContainsKey(x_combo, "to_remove"));
        REQUIRE(comboContainsKey(x_combo, "to_keep"));
    }

    SECTION("observer cleaned up on destruction - no crash")
    {
        auto dm = createDataManager();

        {
            auto state = std::make_shared<ScatterPlotState>();
            ScatterPlotPropertiesWidget widget(state, dm);
            addAnalogTimeSeries(*dm, "before_destroy");
            QApplication::processEvents();
        }
        // Widget is destroyed — observer must have been removed

        // Adding more data must NOT crash (no dangling callback)
        addAnalogTimeSeries(*dm, "after_destroy");
        QApplication::processEvents();

        REQUIRE(true);  // If we get here without crashing, test passes
    }
}

TEST_CASE("ScatterPlotPropertiesWidget stale key handling", "[ScatterPlotPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("X source cleared when selected key is deleted")
    {
        auto dm = createDataManager();
        addAnalogTimeSeries(*dm, "signal");

        auto state = std::make_shared<ScatterPlotState>();

        // Pre-configure state with an X source
        ScatterAxisSource x_src;
        x_src.data_key = "signal";
        state->setXSource(x_src);

        ScatterPlotPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        // Verify X source is set
        REQUIRE(state->xSource().has_value());
        REQUIRE(state->xSource()->data_key == "signal");

        // Delete the data
        dm->deleteData("signal");
        QApplication::processEvents();

        // X source should be cleared
        REQUIRE_FALSE(state->xSource().has_value());
    }

    SECTION("Y source cleared when selected key is deleted")
    {
        auto dm = createDataManager();
        addTensorData(*dm, "tensor");

        auto state = std::make_shared<ScatterPlotState>();

        // Pre-configure state with a Y source
        ScatterAxisSource y_src;
        y_src.data_key = "tensor";
        y_src.tensor_column_name = "col_0";
        state->setYSource(y_src);

        ScatterPlotPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        // Verify Y source is set
        REQUIRE(state->ySource().has_value());
        REQUIRE(state->ySource()->data_key == "tensor");

        // Delete the data
        dm->deleteData("tensor");
        QApplication::processEvents();

        // Y source should be cleared
        REQUIRE_FALSE(state->ySource().has_value());
    }

    SECTION("other axis source preserved when only one key deleted")
    {
        auto dm = createDataManager();
        addAnalogTimeSeries(*dm, "x_signal");
        addAnalogTimeSeries(*dm, "y_signal");

        auto state = std::make_shared<ScatterPlotState>();

        // Configure both sources
        ScatterAxisSource x_src;
        x_src.data_key = "x_signal";
        state->setXSource(x_src);

        ScatterAxisSource y_src;
        y_src.data_key = "y_signal";
        state->setYSource(y_src);

        ScatterPlotPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        // Delete only X data
        dm->deleteData("x_signal");
        QApplication::processEvents();

        // X should be cleared, Y preserved
        REQUIRE_FALSE(state->xSource().has_value());
        REQUIRE(state->ySource().has_value());
        REQUIRE(state->ySource()->data_key == "y_signal");
    }
}

TEST_CASE("ScatterPlotPropertiesWidget state synchronization", "[ScatterPlotPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("UI reflects state on construction")
    {
        auto dm = createDataManager();
        addAnalogTimeSeries(*dm, "preset_signal");

        // Configure state before creating widget
        auto state = std::make_shared<ScatterPlotState>();
        ScatterAxisSource x_src;
        x_src.data_key = "preset_signal";
        x_src.time_offset = 5;
        state->setXSource(x_src);
        state->setShowReferenceLine(true);

        ScatterPlotPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(x_combo != nullptr);
        // Should have the preset key selected
        REQUIRE(x_combo->currentText().toStdString().find("preset_signal") != std::string::npos);
    }

    SECTION("widget constructed with null DataManager does not crash")
    {
        auto state = std::make_shared<ScatterPlotState>();
        ScatterPlotPropertiesWidget widget(state, nullptr);

        auto * x_combo = findXKeyCombo(&widget);
        REQUIRE(x_combo != nullptr);
        // Should have only the empty placeholder
        REQUIRE(x_combo->count() == 1);
    }
}
