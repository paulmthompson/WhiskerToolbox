#include <catch2/catch_test_macros.hpp>

#include "Core/MediaWidgetState.hpp"
#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QApplication>

#include <array>
#include <memory>
#include <numeric>
#include <vector>

namespace {

void ensureQtApplication() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
}

std::shared_ptr<DataManager> createDataManager(int num_frames = 10) {
    auto dm = std::make_shared<DataManager>();

    std::vector<int> times(num_frames);
    std::iota(times.begin(), times.end(), 0);
    dm->setTime(TimeKey("time"), std::make_shared<TimeFrame>(times), true);

    return dm;
}

void setupWindow(Media_Window & window,
                 MediaWidgetState & state,
                 std::shared_ptr<DataManager> const & data_manager,
                 ImageSize const & image_size) {
    window.setMediaWidgetState(&state);
    window.setCanvasSize(image_size);
    window.resolveCanvasCoordinateSystem();

    auto time_frame = data_manager->getTime(TimeKey("time"));
    REQUIRE(time_frame != nullptr);
    state.current_position = TimePosition(TimeFrameIndex{0}, time_frame);
}

void enableLine(MediaWidgetState & state, std::string const & key) {
    auto * opts = state.displayOptions().getMutable<LineDisplayOptions>(QString::fromStdString(key));
    REQUIRE(opts != nullptr);
    opts->is_visible() = true;
}

void enablePoint(MediaWidgetState & state, std::string const & key) {
    auto * opts = state.displayOptions().getMutable<PointDisplayOptions>(QString::fromStdString(key));
    REQUIRE(opts != nullptr);
    opts->is_visible() = true;
}

}// namespace

TEST_CASE("Media_Window getSceneDiagnostics reports plotted element counts", "[Media_Window][Diagnostics]") {
    ensureQtApplication();

    auto data_manager = createDataManager();
    data_manager->setData<LineData>("line_a", TimeKey("time"));
    data_manager->setData<PointData>("points_a", TimeKey("time"));

    auto line_data = data_manager->getData<LineData>("line_a");
    auto point_data = data_manager->getData<PointData>("points_a");
    REQUIRE(line_data != nullptr);
    REQUIRE(point_data != nullptr);

    ImageSize const image_size{.width = 640, .height = 480};
    line_data->setImageSize(image_size);
    point_data->setImageSize(image_size);

    Line2D line({Point2D<float>{100.0f, 150.0f}, Point2D<float>{200.0f, 150.0f}});
    line_data->addAtTime(TimeFrameIndex{0}, line, NotifyObservers::No);

    Point2D<float> point{150.0f, 120.0f};
    point_data->addAtTime(TimeFrameIndex{0}, point, NotifyObservers::No);

    MediaWidgetState state;
    Media_Window window(data_manager);
    setupWindow(window, state, data_manager, image_size);
    window.addLineDataToScene("line_a");
    window.addPointDataToScene("points_a");
    enableLine(state, "line_a");
    enablePoint(state, "points_a");

    window.UpdateCanvas();

    auto const diagnostics = window.getSceneDiagnostics();

    REQUIRE(diagnostics.line_paths >= 1);
    REQUIRE(diagnostics.points >= 1);
    REQUIRE(diagnostics.total_scene_items >= diagnostics.line_paths + diagnostics.points);
}
