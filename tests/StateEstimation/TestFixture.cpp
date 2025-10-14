#include "TestFixture.hpp"

TestFixture::TestFixture(double area_width, double area_height)
    : area_width(area_width), area_height(area_height) {}

void TestFixture::add_point(const Point& p, const Velocity& v) {
    ground_truth.emplace_back();
    ground_truth.back().push_back({p, v});
    modifiers.emplace_back(nullptr);
}

void TestFixture::add_modifier(int point_index, std::unique_ptr<MovementModifier> modifier) {
    if (point_index >= 0 && point_index < modifiers.size()) {
        modifiers[point_index] = std::move(modifier);
    }
}

void TestFixture::step(double dt) {
    for (size_t i = 0; i < ground_truth.size(); ++i) {
        if (!ground_truth[i].empty()) {
            PointState new_state = ground_truth[i].back();
            update_point_state(i, new_state, dt);
            ground_truth[i].push_back(new_state);
        }
    }
}

const std::vector<PointState>& TestFixture::get_ground_truth(int point_index) const {
    return ground_truth[point_index];
}

void TestFixture::update_point_state(size_t point_index, PointState& state, double dt) {
    // Apply velocity
    state.p.x += state.v.vx * dt;
    state.p.y += state.v.vy * dt;

    // Boundary bouncing
    if (state.p.x < 0) {
        state.p.x = -state.p.x;
        state.v.vx *= -1.0;
    } else if (state.p.x > area_width) {
        state.p.x = 2 * area_width - state.p.x;
        state.v.vx *= -1.0;
    }

    if (state.p.y < 0) {
        state.p.y = -state.p.y;
        state.v.vy *= -1.0;
    } else if (state.p.y > area_height) {
        state.p.y = 2 * area_height - state.p.y;
        state.v.vy *= -1.0;
    }

    // Apply modifiers
    if (modifiers[point_index]) {
        modifiers[point_index]->apply(state, dt);
    }
}
