#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Media_Widget/UI/MediaRulerViewport.hpp"

#include <QGraphicsScene>
#include <QGraphicsView>

using Catch::Matchers::WithinAbs;

TEST_CASE("computeVisibleMediaViewport maps scene bounds to media pixels", "[MediaRulerViewport]") {
    QGraphicsScene scene(0, 0, 800, 600);
    QGraphicsView view(&scene);
    view.resize(400, 300);
    view.show();
    view.scale(2.0, 2.0);

    float const x_aspect = 2.0f;
    float const y_aspect = 1.5f;

    auto const vp = computeVisibleMediaViewport(view, x_aspect, y_aspect);

    CHECK(vp.min_x <= vp.max_x);
    CHECK(vp.min_y <= vp.max_y);
    CHECK_THAT(vp.max_x - vp.min_x, WithinAbs(400.0 / x_aspect, 5.0));
    CHECK_THAT(vp.max_y - vp.min_y, WithinAbs(300.0 / y_aspect, 5.0));
}

TEST_CASE("computeVisibleMediaViewport returns zeros for invalid aspect", "[MediaRulerViewport]") {
    QGraphicsScene scene;
    QGraphicsView view(&scene);

    auto const vp = computeVisibleMediaViewport(view, 0.0f, 1.0f);
    CHECK(vp.min_x == 0.0);
    CHECK(vp.max_x == 0.0);
}
