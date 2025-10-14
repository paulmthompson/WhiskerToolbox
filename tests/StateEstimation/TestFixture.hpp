#ifndef TEST_FIXTURE_HPP
#define TEST_FIXTURE_HPP

#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cmath>
#include <memory>

// A simple 2D point structure
struct Point {
    double x, y;
};

// A simple 2D velocity structure
struct Velocity {
    double vx, vy;
};

// Represents the state of a point at a given time
struct PointState {
    Point p;
    Velocity v;
};

// A modifier for a point's movement
class MovementModifier {
public:
    virtual ~MovementModifier() = default;
    virtual void apply(PointState& state, double dt) = 0;
};

// A test fixture for simulating point movements
class TestFixture {
public:
    TestFixture(double area_width, double area_height);

    void add_point(const Point& p, const Velocity& v);
    void add_modifier(int point_index, std::unique_ptr<MovementModifier> modifier);

    void step(double dt);

    const std::vector<PointState>& get_ground_truth(int point_index) const;

private:
    void update_point_state(size_t point_index, PointState& state, double dt);

    double area_width, area_height;
    std::vector<std::vector<PointState>> ground_truth;
    std::vector<std::unique_ptr<MovementModifier>> modifiers;
};

#endif // TEST_FIXTURE_HPP
