#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Media_Widget/UI/MediaRulerViewport.hpp"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <array>

using Catch::Matchers::WithinAbs;

namespace {

void ensureQtApplication() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
}

}// namespace

TEST_CASE("computeVisibleMediaViewport maps scene bounds to media pixels", "[MediaRulerViewport]") {
    ensureQtApplication();

    QGraphicsScene scene(0, 0, 800, 600);
    QGraphicsView view(&scene);
    view.resize(400, 300);
    view.scale(2.0, 2.0);

    float const x_aspect = 2.0f;
    float const y_aspect = 1.5f;

    auto const * viewport_widget = view.viewport();
    REQUIRE(viewport_widget != nullptr);

    double const zoom = view.transform().m11();
    double const expected_scene_width = static_cast<double>(viewport_widget->width()) / zoom;
    double const expected_scene_height = static_cast<double>(viewport_widget->height()) / zoom;

    auto const vp = computeVisibleMediaViewport(view, x_aspect, y_aspect);

    CHECK(vp.min_x <= vp.max_x);
    CHECK(vp.min_y <= vp.max_y);
    CHECK_THAT(vp.max_x - vp.min_x, WithinAbs(expected_scene_width / x_aspect, 2.0));
    CHECK_THAT(vp.max_y - vp.min_y, WithinAbs(expected_scene_height / y_aspect, 2.0));
}

TEST_CASE("computeVisibleMediaViewport returns zeros for invalid aspect", "[MediaRulerViewport]") {
    ensureQtApplication();

    QGraphicsScene scene;
    QGraphicsView view(&scene);

    auto const vp = computeVisibleMediaViewport(view, 0.0f, 1.0f);
    CHECK(vp.min_x == 0.0);
    CHECK(vp.max_x == 0.0);
}
