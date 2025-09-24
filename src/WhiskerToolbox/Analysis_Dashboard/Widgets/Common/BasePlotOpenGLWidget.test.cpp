#include "Analysis_Dashboard/Widgets/Common/BasePlotOpenGLWidget.hpp"
#include "../fixtures/qt_test_fixtures.hpp"
#include "CoreGeometry/points.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QPoint>
#include <QRegularExpression>
#include <QTextStream>
#include <QVector2D>

#include <algorithm>
#include <vector>

namespace {

/**
 * @brief Minimal derived plot widget for testing view transformations.
 */
class TestPlotWidget : public BasePlotOpenGLWidget {
public:
    /**
     * @brief Construct a TestPlotWidget with deterministic point data.
     */
    TestPlotWidget()
        : BasePlotOpenGLWidget(nullptr),
          _points({Point2D<float>{1.0f, 2.0f}, Point2D<float>{7.0f, 8.0f}, Point2D<float>{4.0f, 5.0f}}),
          _bounding_box{1.0f, 2.0f, 7.0f, 8.0f} {
        auto & view_state = getViewState();
        view_state.padding_factor = 1.0f;
        calculateDataBounds();
    }

    /**
     * @brief Get the expected data bounds for verification.
     * @return Bounding box enclosing the test points.
     */
    [[nodiscard]] BoundingBox getExpectedBounds() const { return _bounding_box; }

    [[nodiscard]] std::optional<QString> generateTooltipContent(QPoint const & screen_pos) const override {
        if (!_tooltips_enabled) {
            return std::nullopt;
        }

        QPoint local_pos = rect().contains(screen_pos) ? screen_pos : mapFromGlobal(screen_pos);
        if (!rect().contains(local_pos)) {
            return std::nullopt;
        }

        auto world = screenToWorld(local_pos);
        QString text;
        QTextStream stream(&text);
        stream.setRealNumberNotation(QTextStream::FixedNotation);
        stream.setRealNumberPrecision(3);
        stream << "Position: (" << world.x() << ", " << world.y() << ")";
        return text;
    }

protected:
    void renderData() override {}

    void calculateDataBounds() override {
        if (_points.empty()) {
            _bounding_box = BoundingBox{0.0f, 0.0f, 0.0f, 0.0f};
            return;
        }

        auto [min_x_it, max_x_it] = std::minmax_element(_points.begin(), _points.end(),
                                                        [](Point2D<float> const & lhs, Point2D<float> const & rhs) {
                                                            return lhs.x < rhs.x;
                                                        });
        auto [min_y_it, max_y_it] = std::minmax_element(_points.begin(), _points.end(),
                                                        [](Point2D<float> const & lhs, Point2D<float> const & rhs) {
                                                            return lhs.y < rhs.y;
                                                        });

        _bounding_box = BoundingBox{min_x_it->x, min_y_it->y, max_x_it->x, max_y_it->y};
    }

    [[nodiscard]] BoundingBox getDataBounds() const override { return _bounding_box; }

    void doSetGroupManager(GroupManager *) override {}
    void clearSelection() override {}

private:
    std::vector<Point2D<float>> _points;
    BoundingBox _bounding_box;
};

}// namespace

TEST_CASE_METHOD(QtWidgetTestFixture,
                 "Analysis Dashboard - BasePlotOpenGLWidget - screenToWorld maps widget corners",
                 "[BasePlotOpenGLWidget][ViewState]") {
    TestPlotWidget widget;
    widget.resize(400, 400);
    widget.show();
    processEvents();

    widget.resetView();
    processEvents();

    auto const bounds = widget.getExpectedBounds();

    REQUIRE(widget.getViewState().data_bounds_valid);

    auto top_left = widget.screenToWorld(QPoint(0, 0));
    REQUIRE(top_left.x() == Catch::Approx(bounds.min_x).margin(1e-4f));
    REQUIRE(top_left.y() == Catch::Approx(bounds.max_y).margin(1e-4f));

    auto top_right = widget.screenToWorld(QPoint(widget.width(), 0));
    REQUIRE(top_right.x() == Catch::Approx(bounds.max_x).margin(1e-4f));
    REQUIRE(top_right.y() == Catch::Approx(bounds.max_y).margin(1e-4f));

    auto bottom_left = widget.screenToWorld(QPoint(0, widget.height()));
    REQUIRE(bottom_left.x() == Catch::Approx(bounds.min_x).margin(1e-4f));
    REQUIRE(bottom_left.y() == Catch::Approx(bounds.min_y).margin(1e-4f));

    auto bottom_right = widget.screenToWorld(QPoint(widget.width(), widget.height()));
    REQUIRE(bottom_right.x() == Catch::Approx(bounds.max_x).margin(1e-4f));
    REQUIRE(bottom_right.y() == Catch::Approx(bounds.min_y).margin(1e-4f));

    auto center = widget.screenToWorld(QPoint(widget.width() / 2, widget.height() / 2));
    REQUIRE(center.x() == Catch::Approx(bounds.center_x()).margin(1e-4f));
    REQUIRE(center.y() == Catch::Approx(bounds.center_y()).margin(1e-4f));
}
