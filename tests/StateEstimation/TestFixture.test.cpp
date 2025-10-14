#include "TestFixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>

// A simple modifier that adds a constant acceleration
class ConstantAccelerationModifier : public MovementModifier {
public:
    ConstantAccelerationModifier(const Velocity& a) : a(a) {}

    void apply(PointState& state, double dt) override {
        state.v.vx += a.vx * dt;
        state.v.vy += a.vy * dt;
    }

private:
    Velocity a;
};

TEST_CASE("TestFixture: Basic point movement", "[TestFixture]") {
    TestFixture fixture(100.0, 100.0);
    fixture.add_point({50.0, 50.0}, {10.0, 0.0});

    fixture.step(1.0);

    const auto& ground_truth = fixture.get_ground_truth(0);
    REQUIRE(ground_truth.size() == 2);

    const auto& last_state = ground_truth.back();
    REQUIRE_THAT(last_state.p.x, Catch::Matchers::WithinAbs(60.0, 1e-6));
    REQUIRE_THAT(last_state.p.y, Catch::Matchers::WithinAbs(50.0, 1e-6));
}

TEST_CASE("TestFixture: Boundary bouncing", "[TestFixture]") {
    TestFixture fixture(100.0, 100.0);
    fixture.add_point({95.0, 50.0}, {10.0, 0.0});

    fixture.step(1.0); // Move to 105.0, bounce

    const auto& ground_truth = fixture.get_ground_truth(0);
    REQUIRE(ground_truth.size() == 2);

    const auto& last_state = ground_truth.back();
    REQUIRE(last_state.v.vx == -10.0);
    REQUIRE_THAT(last_state.p.x, Catch::Matchers::WithinAbs(95.0, 1e-6));
}

TEST_CASE("TestFixture: Movement with modifier", "[TestFixture]") {
    TestFixture fixture(100.0, 100.0);
    fixture.add_point({50.0, 50.0}, {10.0, 0.0});

    auto modifier = std::make_unique<ConstantAccelerationModifier>(Velocity{0.0, 5.0});
    fixture.add_modifier(0, std::move(modifier));

    fixture.step(1.0);

    const auto& ground_truth = fixture.get_ground_truth(0);
    REQUIRE(ground_truth.size() == 2);

    const auto& last_state = ground_truth.back();
    REQUIRE_THAT(last_state.p.x, Catch::Matchers::WithinAbs(60.0, 1e-6));
    REQUIRE_THAT(last_state.p.y, Catch::Matchers::WithinAbs(50.0, 1e-6));
    REQUIRE_THAT(last_state.v.vx, Catch::Matchers::WithinAbs(10.0, 1e-6));
    REQUIRE_THAT(last_state.v.vy, Catch::Matchers::WithinAbs(5.0, 1e-6));
}
