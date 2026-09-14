#include <catch2/catch_test_macros.hpp>

#include "Core/MediaWidgetState.hpp"
#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "Lines/Line_Data.hpp"
#include "Media_Widget/UI/Media_Widget.hpp"
#include "Media_Widget/UI/Tools/SelectToolOptions_Widget.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QApplication>
#include <QComboBox>

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

QComboBox * findFilterCombo(SelectToolOptions_Widget const & widget) {
    return widget.findChild<QComboBox *>();
}

std::shared_ptr<DataManager> createDataManagerWithLine(std::string const & line_key) {
    auto dm = std::make_shared<DataManager>();

    std::vector<int> times(10);
    std::iota(times.begin(), times.end(), 0);
    dm->setTime(TimeKey("time"), std::make_shared<TimeFrame>(times), true);
    dm->setData<LineData>(line_key, TimeKey("time"));

    auto line_data = dm->getData<LineData>(line_key);
    if (line_data) {
        line_data->setImageSize(ImageSize{.width = 640, .height = 480});
    }

    return dm;
}

}// namespace

TEST_CASE("SelectToolOptions_Widget lists enabled features in filter combo", "[SelectToolOptions]") {
    ensureQtApplication();

    auto state = std::make_shared<MediaWidgetState>();
    state->setFeatureEnabled(QStringLiteral("whisker_a"), QStringLiteral("line"), true);
    state->setFeatureEnabled(QStringLiteral("whisker_b"), QStringLiteral("line"), false);
    state->setFeatureEnabled(QStringLiteral("contact_1"), QStringLiteral("point"), true);

    SelectToolOptions_Widget widget;
    widget.setState(state.get());

    auto * combo = findFilterCombo(widget);
    REQUIRE(combo != nullptr);
    REQUIRE(combo->count() == 3);
    REQUIRE(combo->itemText(0) == QStringLiteral("All enabled features"));
    REQUIRE(combo->itemText(1) == QStringLiteral("line · whisker_a"));
    REQUIRE(combo->itemText(2) == QStringLiteral("point · contact_1"));
}

TEST_CASE("SelectToolOptions_Widget updates SelectToolPrefs when filter changes", "[SelectToolOptions]") {
    ensureQtApplication();

    auto state = std::make_shared<MediaWidgetState>();
    state->setFeatureEnabled(QStringLiteral("whisker_a"), QStringLiteral("line"), true);

    SelectToolOptions_Widget widget;
    widget.setState(state.get());

    auto * combo = findFilterCombo(widget);
    REQUIRE(combo != nullptr);

    combo->setCurrentIndex(1);

    SelectToolPrefs const & prefs = state->selectPrefs();
    REQUIRE(prefs.filter_to_key);
    REQUIRE(prefs.filter_key == "whisker_a");
    REQUIRE(prefs.filter_data_type == "line");

    combo->setCurrentIndex(0);
    REQUIRE_FALSE(state->selectPrefs().filter_to_key);
}

TEST_CASE("SelectToolOptions_Widget rebuilds when visibility changes via setVisible", "[SelectToolOptions]") {
    ensureQtApplication();

    auto state = std::make_shared<MediaWidgetState>();
    state->setFeatureEnabled(QStringLiteral("whisker_a"), QStringLiteral("line"), true);
    state->setFeatureEnabled(QStringLiteral("whisker_b"), QStringLiteral("line"), true);

    SelectToolOptions_Widget widget;
    widget.setState(state.get());

    auto * combo = findFilterCombo(widget);
    REQUIRE(combo != nullptr);
    REQUIRE(combo->count() == 3);

    state->displayOptions().setVisible(QStringLiteral("whisker_b"), QStringLiteral("line"), false);
    REQUIRE(combo->count() == 2);

    state->setFeatureEnabled(QStringLiteral("contact_1"), QStringLiteral("point"), true);
    REQUIRE(combo->count() == 3);
    REQUIRE(combo->itemText(2) == QStringLiteral("point · contact_1"));

    state->displayOptions().setVisible(QStringLiteral("contact_1"), QStringLiteral("point"), false);
    REQUIRE(combo->count() == 2);
}

TEST_CASE("SelectToolOptions_Widget removes deleted DataManager key from filter combo",
          "[SelectToolOptions][DataManager]") {
    ensureQtApplication();
    auto * app = QApplication::instance();

    auto data_manager = createDataManagerWithLine("whisker_a");
    data_manager->setData<LineData>("whisker_b", TimeKey("time"));
    auto line_b = data_manager->getData<LineData>("whisker_b");
    if (line_b) {
        line_b->setImageSize(ImageSize{.width = 640, .height = 480});
    }

    EditorRegistry editor_registry(nullptr);
    Media_Widget media_widget(&editor_registry);
    media_widget.setDataManager(data_manager);

    auto state = media_widget.getState();
    REQUIRE(state != nullptr);

    state->displayOptions().setVisible(QStringLiteral("whisker_a"), QStringLiteral("line"), true);
    state->displayOptions().setVisible(QStringLiteral("whisker_b"), QStringLiteral("line"), true);

    SelectToolOptions_Widget const * options_widget =
            media_widget.findChild<SelectToolOptions_Widget *>();
    REQUIRE(options_widget != nullptr);

    auto * combo = findFilterCombo(*options_widget);
    REQUIRE(combo != nullptr);
    REQUIRE(combo->count() == 3);

    SelectToolPrefs filter_prefs;
    filter_prefs.filter_to_key = true;
    filter_prefs.filter_key = "whisker_a";
    filter_prefs.filter_data_type = "line";
    state->setSelectPrefs(filter_prefs);
    REQUIRE(combo->currentIndex() == 1);

    REQUIRE(data_manager->deleteData("whisker_a"));
    app->processEvents();

    REQUIRE(combo->count() == 2);
    REQUIRE(combo->itemText(1) == QStringLiteral("line · whisker_b"));
    REQUIRE_FALSE(state->selectPrefs().filter_to_key);
}
