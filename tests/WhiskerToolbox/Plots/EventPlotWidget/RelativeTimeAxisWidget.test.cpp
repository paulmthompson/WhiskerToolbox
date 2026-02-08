/**
 * @file RelativeTimeAxisWidget.test.cpp
 * @brief Integration tests for RelativeTimeAxisWidget with EventPlotWidget
 * 
 * Tests that the RelativeTimeAxisWidget correctly updates its labels when
 * the window size changes in EventPlotState.
 */

#include "Plots/EventPlotWidget/UI/EventPlotWidget.hpp"
#include "Plots/EventPlotWidget/Core/EventPlotState.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include <QApplication>
#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <memory>

// ==================== Helper Functions ====================

/**
 * @brief Create a test DataManager
 */
static std::shared_ptr<DataManager> createTestDataManager()
{
    return std::make_shared<DataManager>();
}

// ==================== Window Size Change Tests ====================

TEST_CASE("RelativeTimeAxisWidget updates when window size changes", "[EventPlotWidget][RelativeTimeAxisWidget]") {
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("Window size change updates view state bounds") {
        auto data_manager = createTestDataManager();
        auto state = std::make_shared<EventPlotState>();

        // Verify initial view state bounds (default is -500 to +500)
        auto const & initial_view_state = state->viewState();
        REQUIRE(initial_view_state.x_min == -500.0);
        REQUIRE(initial_view_state.x_max == 500.0);

        // Set up signal spy for viewStateChanged
        QSignalSpy view_state_changed_spy(state.get(), &EventPlotState::viewStateChanged);

        // Change window size to 2000, which should give -1000 to +1000
        state->setWindowSize(2000.0);
        QApplication::processEvents();

        // Verify signal was emitted
        REQUIRE(view_state_changed_spy.count() >= 1);

        // Verify view state bounds were updated
        auto const & updated_view_state = state->viewState();
        REQUIRE(updated_view_state.x_min == -1000.0);
        REQUIRE(updated_view_state.x_max == 1000.0);

        // Verify toRuntimeViewState produces correct CorePlotting::ViewState
        auto core_view_state =
            CorePlotting::toRuntimeViewState(updated_view_state, 800, 600);
        REQUIRE(core_view_state.data_bounds_valid);
        REQUIRE(core_view_state.data_bounds.min_x == -1000.0f);
        REQUIRE(core_view_state.data_bounds.max_x == 1000.0f);
    }

    SECTION("Multiple window size changes update bounds correctly") {
        auto data_manager = createTestDataManager();
        auto state = std::make_shared<EventPlotState>();

        // Test multiple window sizes
        struct TestCase {
            double window_size;
            double expected_min;
            double expected_max;
        };

        std::vector<TestCase> test_cases = {
            {1000.0, -500.0, 500.0},
            {2000.0, -1000.0, 1000.0},
            {500.0, -250.0, 250.0},
            {3000.0, -1500.0, 1500.0}
        };

        for (auto const & test_case : test_cases) {
            state->setWindowSize(test_case.window_size);
            QApplication::processEvents();

            // Verify view state
            auto const & view_state = state->viewState();
            REQUIRE(view_state.x_min == Catch::Approx(test_case.expected_min));
            REQUIRE(view_state.x_max == Catch::Approx(test_case.expected_max));

            // Verify toRuntimeViewState produces correct bounds
            auto core_view_state =
                CorePlotting::toRuntimeViewState(view_state, 800, 600);
            REQUIRE(core_view_state.data_bounds.min_x ==
                    Catch::Approx(test_case.expected_min));
            REQUIRE(core_view_state.data_bounds.max_x ==
                    Catch::Approx(test_case.expected_max));
        }
    }

    SECTION("ViewStateChanged signal is emitted when window size changes") {
        auto state = std::make_shared<EventPlotState>();

        // Set up signal spy
        QSignalSpy view_state_changed_spy(state.get(), &EventPlotState::viewStateChanged);
        QSignalSpy window_size_changed_spy(state.get(), &EventPlotState::windowSizeChanged);

        // Change window size
        state->setWindowSize(1500.0);
        QApplication::processEvents();

        // Both signals should be emitted
        REQUIRE(view_state_changed_spy.count() >= 1);
        REQUIRE(window_size_changed_spy.count() >= 1);
        
        // Verify the windowSizeChanged signal has correct value
        auto args = window_size_changed_spy.takeFirst();
        REQUIRE(args.at(0).toDouble() == 1500.0);
    }
    
    SECTION("EventPlotWidget creates RelativeTimeAxisWidget with ViewStateGetter") {
        auto data_manager = createTestDataManager();
        auto state = std::make_shared<EventPlotState>();

        // Create widget and set state
        EventPlotWidget widget(data_manager);
        widget.setState(state);
        QApplication::processEvents();

        // Verify the axis widget exists
        auto * axis_widget = widget.findChild<RelativeTimeAxisWidget *>();
        REQUIRE(axis_widget != nullptr);
        
        // Change window size and verify state is updated
        state->setWindowSize(2000.0);
        QApplication::processEvents();
        
        // Verify the view state bounds are updated
        auto const & view_state = state->viewState();
        REQUIRE(view_state.x_min == -1000.0);
        REQUIRE(view_state.x_max == 1000.0);
    }
}
