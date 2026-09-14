#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Core/MediaWidgetState.hpp"
#include "DataManager/DataManager.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "Lines/Line_Data.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "Media_Widget/Selection/MediaSelectionHit.hpp"
#include "Media_Widget/UI/MediaPropertiesWidget.hpp"
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

TEST_CASE("findBestEntityAtPosition prefers closest point over farther line", "[MediaSelectionHit]") {
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

    SelectToolPrefs prefs;
    auto hit = window.findBestEntityAtPosition(QPointF{150.0, 122.0}, prefs);

    REQUIRE(hit.has_value());
    REQUIRE(hit->data_type == "point");
    REQUIRE(hit->data_key == "points_a");
}

TEST_CASE("findBestEntityAtPosition picks closest line across multiple keys", "[MediaSelectionHit]") {
    ensureQtApplication();

    auto data_manager = createDataManager();
    data_manager->setData<LineData>("line_near", TimeKey("time"));
    data_manager->setData<LineData>("line_far", TimeKey("time"));

    auto near_line = data_manager->getData<LineData>("line_near");
    auto far_line = data_manager->getData<LineData>("line_far");
    REQUIRE(near_line != nullptr);
    REQUIRE(far_line != nullptr);

    ImageSize const image_size{.width = 640, .height = 480};
    near_line->setImageSize(image_size);
    far_line->setImageSize(image_size);

    near_line->addAtTime(TimeFrameIndex{0},
                         Line2D({Point2D<float>{100.0f, 100.0f}, Point2D<float>{200.0f, 100.0f}}),
                         NotifyObservers::No);
    far_line->addAtTime(TimeFrameIndex{0},
                        Line2D({Point2D<float>{100.0f, 200.0f}, Point2D<float>{200.0f, 200.0f}}),
                        NotifyObservers::No);

    MediaWidgetState state;
    Media_Window window(data_manager);
    setupWindow(window, state, data_manager, image_size);
    window.addLineDataToScene("line_near");
    window.addLineDataToScene("line_far");
    enableLine(state, "line_near");
    enableLine(state, "line_far");

    SelectToolPrefs prefs;
    auto hit = window.findBestEntityAtPosition(QPointF{150.0, 105.0}, prefs);

    REQUIRE(hit.has_value());
    REQUIRE(hit->data_key == "line_near");
    REQUIRE(hit->data_type == "line");
}

TEST_CASE("findBestEntityAtPosition honors SelectToolPrefs key filter", "[MediaSelectionHit]") {
    ensureQtApplication();

    auto data_manager = createDataManager();
    data_manager->setData<LineData>("line_near", TimeKey("time"));
    data_manager->setData<LineData>("line_far", TimeKey("time"));

    auto near_line = data_manager->getData<LineData>("line_near");
    auto far_line = data_manager->getData<LineData>("line_far");
    REQUIRE(near_line != nullptr);
    REQUIRE(far_line != nullptr);

    ImageSize const image_size{.width = 640, .height = 480};
    near_line->setImageSize(image_size);
    far_line->setImageSize(image_size);

    near_line->addAtTime(TimeFrameIndex{0},
                         Line2D({Point2D<float>{100.0f, 100.0f}, Point2D<float>{200.0f, 100.0f}}),
                         NotifyObservers::No);
    far_line->addAtTime(TimeFrameIndex{0},
                        Line2D({Point2D<float>{100.0f, 200.0f}, Point2D<float>{200.0f, 200.0f}}),
                        NotifyObservers::No);

    MediaWidgetState state;
    Media_Window window(data_manager);
    setupWindow(window, state, data_manager, image_size);
    window.addLineDataToScene("line_near");
    window.addLineDataToScene("line_far");
    enableLine(state, "line_near");
    enableLine(state, "line_far");

    SelectToolPrefs prefs;
    prefs.filter_to_key = true;
    prefs.filter_key = "line_far";
    prefs.filter_data_type = "line";

    auto hit_near_click = window.findBestEntityAtPosition(QPointF{150.0, 105.0}, prefs);
    REQUIRE_FALSE(hit_near_click.has_value());

    auto hit_far_click = window.findBestEntityAtPosition(QPointF{150.0, 205.0}, prefs);
    REQUIRE(hit_far_click.has_value());
    REQUIRE(hit_far_click->data_key == "line_far");
}

TEST_CASE("Feature_Table_Widget selectFeature highlights row without emitting featureSelected",
          "[MediaSelectionHit][FeatureTable]") {
    ensureQtApplication();

    auto data_manager = createDataManager();
    data_manager->setData<LineData>("whisker_a", TimeKey("time"));
    data_manager->setData<LineData>("whisker_b", TimeKey("time"));

    auto state = std::make_shared<MediaWidgetState>();
    MediaPropertiesWidget props_widget(state, data_manager, nullptr);
    props_widget.resize(900, 700);
    props_widget.show();

    auto * feature_table = props_widget.findChild<Feature_Table_Widget *>();
    REQUIRE(feature_table != nullptr);
    feature_table->populateTable();

    bool feature_selected_emitted = false;
    QObject::connect(feature_table, &Feature_Table_Widget::featureSelected, [&]() {
        feature_selected_emitted = true;
    });

    feature_table->selectFeature(QStringLiteral("whisker_b"));

    REQUIRE(feature_table->getHighlightedFeature() == QStringLiteral("whisker_b"));
    REQUIRE_FALSE(feature_selected_emitted);
}
