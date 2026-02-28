/**
 * @file PlotWidgetCommonTests.hpp
 * @brief Template-based common tests for plot properties widgets
 *
 * This header provides reusable test logic for all plot properties widgets
 * that register DataManager observer callbacks. Instead of duplicating
 * identical test code across LinePlotPropertiesWidget.test.cpp,
 * EventPlotPropertiesWidget.test.cpp, ACFPropertiesWidget.test.cpp, etc.,
 * widgets provide a Traits struct and instantiate
 * REGISTER_PLOT_WIDGET_COMMON_TESTS(Traits).
 *
 * ## Trait Requirements
 *
 * A Traits struct must provide:
 *
 * ```cpp
 * struct MyWidgetTraits {
 *     // --- Types ---
 *     using State = MyState;
 *     using PropertiesWidget = MyPropertiesWidget;
 *
 *     // --- Factory: create state ---
 *     static std::shared_ptr<State> createState();
 *
 *     // --- Factory: create properties widget ---
 *     static std::unique_ptr<PropertiesWidget> createWidget(
 *         std::shared_ptr<State> state,
 *         std::shared_ptr<DataManager> dm);
 *
 *     // --- Verify that a combo box in the widget contains the given key ---
 *     static bool comboContainsKey(PropertiesWidget * widget,
 *                                  std::string const & key);
 *
 *     // --- Count items in the relevant combo box ---
 *     static int comboCount(PropertiesWidget * widget);
 * };
 * ```
 *
 * ## Usage
 *
 * In a .cpp test file:
 * ```cpp
 * #include "PlotWidgetCommonTests.hpp"
 * // ... include widget headers ...
 *
 * struct MyWidgetTraits { ... };
 * REGISTER_PLOT_WIDGET_COMMON_TESTS(MyWidgetTraits)
 * ```
 */

#ifndef PLOT_WIDGET_COMMON_TESTS_HPP
#define PLOT_WIDGET_COMMON_TESTS_HPP

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QApplication>

#include <memory>
#include <string>
#include <vector>

// ── Shared test data helpers ─────────────────────────────────────────────────

namespace PlotWidgetTestHelpers {

inline std::shared_ptr<TimeFrame> createTestTimeFrame()
{
    std::vector<int> times;
    times.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

inline std::shared_ptr<DigitalEventSeries> createTestEventSeries()
{
    std::vector<TimeFrameIndex> events = {
        TimeFrameIndex(100),
        TimeFrameIndex(200),
        TimeFrameIndex(300),
        TimeFrameIndex(400),
        TimeFrameIndex(500)};
    return std::make_shared<DigitalEventSeries>(events);
}

inline std::shared_ptr<DigitalIntervalSeries> createTestIntervalSeries()
{
    std::vector<Interval> intervals = {
        Interval{100, 200},
        Interval{300, 400},
        Interval{500, 600}};
    return std::make_shared<DigitalIntervalSeries>(intervals);
}

/**
 * @brief Set up a DataManager with a time frame and two DigitalEventSeries
 * @return Pair of (time_frame, vector of added key names)
 */
inline std::pair<std::shared_ptr<TimeFrame>, std::vector<std::string>>
setupDataManagerWithEvents(std::shared_ptr<DataManager> const & dm)
{
    dm->removeTime(TimeKey("time"));
    auto tf = createTestTimeFrame();
    dm->setTime(TimeKey("time"), tf);

    auto es1 = createTestEventSeries();
    auto es2 = createTestEventSeries();
    es1->setTimeFrame(tf);
    es2->setTimeFrame(tf);

    dm->setData<DigitalEventSeries>("events_1", es1, TimeKey("time"));
    dm->setData<DigitalEventSeries>("events_2", es2, TimeKey("time"));

    return {tf, {"events_1", "events_2"}};
}

} // namespace PlotWidgetTestHelpers

// ── Macro that generates common tests for a given Traits type ────────────────

// NOLINTBEGIN(bugprone-macro-parentheses,cppcoreguidelines-macro-usage)

/**
 * @brief Register the standard suite of DataManager integration tests for a
 *        plot properties widget.
 *
 * @param TraitsType A struct satisfying the Traits concept documented above.
 */
#define REGISTER_PLOT_WIDGET_COMMON_TESTS(TraitsType)                                              \
                                                                                                   \
    TEST_CASE(#TraitsType " common: observer refresh on data add",                                 \
              "[" #TraitsType "][common]")                                                          \
    {                                                                                              \
        int argc = 0;                                                                              \
        QApplication app(argc, nullptr);                                                           \
                                                                                                   \
        auto dm = std::make_shared<DataManager>();                                                 \
        auto state = TraitsType::createState();                                                    \
        auto widget = TraitsType::createWidget(state, dm);                                         \
                                                                                                   \
        int const initial_count = TraitsType::comboCount(widget.get());                            \
                                                                                                   \
        auto [tf, keys] = PlotWidgetTestHelpers::setupDataManagerWithEvents(dm);                   \
        QApplication::processEvents();                                                             \
                                                                                                   \
        int const after_count = TraitsType::comboCount(widget.get());                              \
        REQUIRE(after_count > initial_count);                                                      \
        for (auto const & k : keys) {                                                              \
            REQUIRE(TraitsType::comboContainsKey(widget.get(), k));                                 \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    TEST_CASE(#TraitsType " common: observer refresh on data remove",                         \
              "[" #TraitsType "][common]")                                                          \
    {                                                                                              \
        int argc = 0;                                                                              \
        QApplication app(argc, nullptr);                                                           \
                                                                                                   \
        auto dm = std::make_shared<DataManager>();                                                 \
        auto [tf, keys] = PlotWidgetTestHelpers::setupDataManagerWithEvents(dm);                   \
        QApplication::processEvents();                                                             \
                                                                                                   \
        auto state = TraitsType::createState();                                                    \
        auto widget = TraitsType::createWidget(state, dm);                                         \
        QApplication::processEvents();                                                             \
                                                                                                   \
        REQUIRE(TraitsType::comboContainsKey(widget.get(), keys.front()));                          \
                                                                                                   \
        dm->deleteData(keys.front());                                                              \
        QApplication::processEvents();                                                             \
                                                                                                   \
        REQUIRE_FALSE(TraitsType::comboContainsKey(widget.get(), keys.front()));                    \
    }                                                                                              \
                                                                                                   \
    TEST_CASE(#TraitsType " common: observer cleaned up on destruction",                      \
              "[" #TraitsType "][common]")                                                          \
    {                                                                                              \
        int argc = 0;                                                                              \
        QApplication app(argc, nullptr);                                                           \
                                                                                                   \
        auto dm = std::make_shared<DataManager>();                                                 \
                                                                                                   \
        {                                                                                          \
            auto state = TraitsType::createState();                                                \
            auto widget = TraitsType::createWidget(state, dm);                                     \
                                                                                                   \
            auto [tf, keys] = PlotWidgetTestHelpers::setupDataManagerWithEvents(dm);               \
            QApplication::processEvents();                                                         \
        }                                                                                          \
        /* Widget is destroyed — observer must have been removed. */                               \
        /* Adding more data must NOT crash (no dangling callback). */                              \
        dm->removeTime(TimeKey("time"));                                                           \
        auto tf2 = PlotWidgetTestHelpers::createTestTimeFrame();                                   \
        dm->setTime(TimeKey("time"), tf2);                                                         \
        auto es = PlotWidgetTestHelpers::createTestEventSeries();                                  \
        es->setTimeFrame(tf2);                                                                     \
        dm->setData<DigitalEventSeries>("after_destroy", es, TimeKey("time"));                     \
        QApplication::processEvents();                                                             \
                                                                                                   \
        REQUIRE(true); /* If we get here without crashing, the test passes. */                     \
    }                                                                                              \
                                                                                                   \
    TEST_CASE(#TraitsType " common: widget constructed with empty DataManager",               \
              "[" #TraitsType "][common]")                                                          \
    {                                                                                              \
        int argc = 0;                                                                              \
        QApplication app(argc, nullptr);                                                           \
                                                                                                   \
        auto dm = std::make_shared<DataManager>();                                                 \
        auto state = TraitsType::createState();                                                    \
        auto widget = TraitsType::createWidget(state, dm);                                         \
                                                                                                   \
        /* Combo should be empty or have only a placeholder like "(None)". */                      \
        REQUIRE(widget != nullptr);                                                                \
        /* Specific count depends on whether widget has a "(None)" placeholder. */                 \
        /* We just verify it doesn't crash and count is small. */                                  \
        REQUIRE(TraitsType::comboCount(widget.get()) <= 1);                                        \
    }

// NOLINTEND(bugprone-macro-parentheses,cppcoreguidelines-macro-usage)

#endif // PLOT_WIDGET_COMMON_TESTS_HPP
