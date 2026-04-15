#include "Observer_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("ObserverData happy path functionality", "[ObserverData][observer_pattern]") {
    ObserverData observer_data;

    SECTION("Adding and notifying single observer") {
        bool callback_called = false;
        auto callback = [&callback_called]() {
            callback_called = true;
        };

        auto id = observer_data.addObserver(callback);
        REQUIRE(id > 0);

        observer_data.notifyObservers();
        REQUIRE(callback_called);
    }

    SECTION("Adding and notifying multiple observers") {
        int callback_count = 0;
        auto callback1 = [&callback_count]() {
            callback_count++;
        };
        auto callback2 = [&callback_count]() {
            callback_count++;
        };

        auto id1 = observer_data.addObserver(callback1);
        auto id2 = observer_data.addObserver(callback2);

        REQUIRE(id1 != id2);
        REQUIRE(id1 > 0);
        REQUIRE(id2 > 0);

        observer_data.notifyObservers();
        REQUIRE(callback_count == 2);
    }

    SECTION("Removing observer stops notifications") {
        bool callback_called = false;
        auto callback = [&callback_called]() {
            callback_called = true;
        };

        auto id = observer_data.addObserver(callback);
        observer_data.removeObserver(id);

        observer_data.notifyObservers();
        REQUIRE_FALSE(callback_called);
    }

    SECTION("Multiple notifications work correctly") {
        int callback_count = 0;
        auto callback = [&callback_count]() {
            callback_count++;
        };

        observer_data.addObserver(callback);

        observer_data.notifyObservers();
        observer_data.notifyObservers();
        observer_data.notifyObservers();

        REQUIRE(callback_count == 3);
    }
}

TEST_CASE("ObserverData edge cases and error handling", "[ObserverData][observer_pattern][edge_cases]") {
    ObserverData observer_data;

    SECTION("Removing non-existent observer ID does not crash") {
        // This should not crash or throw
        observer_data.removeObserver(999);
        observer_data.removeObserver(-1);

        // Should still be able to add and notify observers
        bool callback_called = false;
        auto callback = [&callback_called]() {
            callback_called = true;
        };

        observer_data.addObserver(callback);
        observer_data.notifyObservers();
        REQUIRE(callback_called);
    }

    SECTION("Notifying with no observers does not crash") {
        // This should not crash
        observer_data.notifyObservers();

        // Should still be able to add observers after
        bool callback_called = false;
        auto callback = [&callback_called]() {
            callback_called = true;
        };

        observer_data.addObserver(callback);
        observer_data.notifyObservers();
        REQUIRE(callback_called);
    }

    SECTION("Observer IDs are unique across multiple additions") {
        std::vector<ObserverData::CallbackID> ids;
        auto dummy_callback = []() {};

        // Add multiple observers and ensure all IDs are unique
        for (int i = 0; i < 10; ++i) {
            auto id = observer_data.addObserver(dummy_callback);
            REQUIRE(std::find(ids.begin(), ids.end(), id) == ids.end());
            ids.push_back(id);
        }
    }

    SECTION("Removing observer while others exist works correctly") {
        int callback1_count = 0;
        int callback2_count = 0;

        auto callback1 = [&callback1_count]() {
            callback1_count++;
        };
        auto callback2 = [&callback2_count]() {
            callback2_count++;
        };

        auto id1 = observer_data.addObserver(callback1);
        auto id2 = observer_data.addObserver(callback2);

        // Remove first observer
        observer_data.removeObserver(id1);

        observer_data.notifyObservers();

        REQUIRE(callback1_count == 0);
        REQUIRE(callback2_count == 1);
    }
}

TEST_CASE("ObserverData supports re-entrant modification during notification", "[ObserverData][observer_pattern]") {
    ObserverData observer_data;

    SECTION("Removing self from inside callback does not crash") {
        ObserverData::CallbackID self_id = -1;
        int call_count = 0;
        self_id = observer_data.addObserver([&]() {
            call_count++;
            observer_data.removeObserver(self_id);
        });

        // Should not crash — notifyObservers iterates a snapshot
        observer_data.notifyObservers();
        REQUIRE(call_count == 1);

        // Second call should not invoke the removed callback
        observer_data.notifyObservers();
        REQUIRE(call_count == 1);
    }

    SECTION("Adding observer from inside callback does not crash") {
        int outer_count = 0;
        int inner_count = 0;
        observer_data.addObserver([&]() {
            outer_count++;
            // Add a new observer from within the notification
            observer_data.addObserver([&]() { inner_count++; });
        });

        observer_data.notifyObservers();
        REQUIRE(outer_count == 1);
        // Inner observer was added after the snapshot, so not called this round
        REQUIRE(inner_count == 0);

        // Second round should call both
        observer_data.notifyObservers();
        REQUIRE(outer_count == 2);
        REQUIRE(inner_count == 1);
    }
}
