#include "DisplayOptionsRegistry.hpp"
#include "MediaWidgetStateData.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QSignalSpy>

using Catch::Matchers::WithinAbs;

// ==================== Type Name Tests ====================

TEST_CASE("DisplayOptionsRegistry type names are correct", "[DisplayOptionsRegistry]")
{
    REQUIRE(DisplayOptionsRegistry::typeName<LineDisplayOptions>() == "line");
    REQUIRE(DisplayOptionsRegistry::typeName<MaskDisplayOptions>() == "mask");
    REQUIRE(DisplayOptionsRegistry::typeName<PointDisplayOptions>() == "point");
    REQUIRE(DisplayOptionsRegistry::typeName<TensorDisplayOptions>() == "tensor");
    REQUIRE(DisplayOptionsRegistry::typeName<DigitalIntervalDisplayOptions>() == "interval");
    REQUIRE(DisplayOptionsRegistry::typeName<MediaDisplayOptions>() == "media");
}

// ==================== Line Display Options Tests ====================

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions set/get", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions opts;
    opts.hex_color() = "#ff0000";
    opts.alpha() = 0.8f;
    opts.line_thickness = 5;
    opts.show_points = true;

    registry.set(QStringLiteral("line_1"), opts);

    auto const * retrieved = registry.get<LineDisplayOptions>(QStringLiteral("line_1"));
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->hex_color() == "#ff0000");
    REQUIRE_THAT(retrieved->alpha(), WithinAbs(0.8f, 0.001f));
    REQUIRE(retrieved->line_thickness == 5);
    REQUIRE(retrieved->show_points == true);
}

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions signal emission", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    QSignalSpy changed_spy(&registry, &DisplayOptionsRegistry::optionsChanged);
    REQUIRE(changed_spy.isValid());

    LineDisplayOptions opts;
    registry.set(QStringLiteral("line_1"), opts);

    REQUIRE(changed_spy.count() == 1);
    auto args = changed_spy.takeFirst();
    REQUIRE(args.at(0).toString() == "line_1");
    REQUIRE(args.at(1).toString() == "line");
}

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions remove", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions opts;
    registry.set(QStringLiteral("line_1"), opts);
    REQUIRE(registry.has<LineDisplayOptions>(QStringLiteral("line_1")));

    QSignalSpy removed_spy(&registry, &DisplayOptionsRegistry::optionsRemoved);
    REQUIRE(removed_spy.isValid());

    bool removed = registry.remove<LineDisplayOptions>(QStringLiteral("line_1"));
    REQUIRE(removed);
    REQUIRE_FALSE(registry.has<LineDisplayOptions>(QStringLiteral("line_1")));
    REQUIRE(removed_spy.count() == 1);

    // Removing non-existent key returns false
    removed = registry.remove<LineDisplayOptions>(QStringLiteral("line_1"));
    REQUIRE_FALSE(removed);
}

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions keys", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions opts1, opts2, opts3;
    registry.set(QStringLiteral("line_a"), opts1);
    registry.set(QStringLiteral("line_b"), opts2);
    registry.set(QStringLiteral("line_c"), opts3);

    QStringList keys = registry.keys<LineDisplayOptions>();
    REQUIRE(keys.size() == 3);
    REQUIRE(keys.contains("line_a"));
    REQUIRE(keys.contains("line_b"));
    REQUIRE(keys.contains("line_c"));
}

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions enabledKeys", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions opts1, opts2, opts3;
    opts1.is_visible() = true;
    opts2.is_visible() = false;
    opts3.is_visible() = true;

    registry.set(QStringLiteral("line_a"), opts1);
    registry.set(QStringLiteral("line_b"), opts2);
    registry.set(QStringLiteral("line_c"), opts3);

    QStringList enabled = registry.enabledKeys<LineDisplayOptions>();
    REQUIRE(enabled.size() == 2);
    REQUIRE(enabled.contains("line_a"));
    REQUIRE_FALSE(enabled.contains("line_b"));
    REQUIRE(enabled.contains("line_c"));
}

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions count", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    REQUIRE(registry.count<LineDisplayOptions>() == 0);

    LineDisplayOptions opts;
    registry.set(QStringLiteral("line_1"), opts);
    REQUIRE(registry.count<LineDisplayOptions>() == 1);

    registry.set(QStringLiteral("line_2"), opts);
    REQUIRE(registry.count<LineDisplayOptions>() == 2);

    registry.remove<LineDisplayOptions>(QStringLiteral("line_1"));
    REQUIRE(registry.count<LineDisplayOptions>() == 1);
}

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions getMutable", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions opts;
    opts.line_thickness = 2;
    registry.set(QStringLiteral("line_1"), opts);

    auto * mutable_opts = registry.getMutable<LineDisplayOptions>(QStringLiteral("line_1"));
    REQUIRE(mutable_opts != nullptr);
    mutable_opts->line_thickness = 10;

    auto const * retrieved = registry.get<LineDisplayOptions>(QStringLiteral("line_1"));
    REQUIRE(retrieved->line_thickness == 10);
}

TEST_CASE("DisplayOptionsRegistry LineDisplayOptions notifyChanged", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions opts;
    registry.set(QStringLiteral("line_1"), opts);

    QSignalSpy changed_spy(&registry, &DisplayOptionsRegistry::optionsChanged);
    REQUIRE(changed_spy.isValid());

    registry.notifyChanged<LineDisplayOptions>(QStringLiteral("line_1"));

    REQUIRE(changed_spy.count() == 1);
    auto args = changed_spy.takeFirst();
    REQUIRE(args.at(0).toString() == "line_1");
    REQUIRE(args.at(1).toString() == "line");
}

// ==================== Mask Display Options Tests ====================

TEST_CASE("DisplayOptionsRegistry MaskDisplayOptions set/get", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    MaskDisplayOptions opts;
    opts.hex_color() = "#00ff00";
    opts.alpha() = 0.5f;
    opts.show_bounding_box = true;
    opts.show_outline = true;

    registry.set(QStringLiteral("mask_1"), opts);

    auto const * retrieved = registry.get<MaskDisplayOptions>(QStringLiteral("mask_1"));
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->hex_color() == "#00ff00");
    REQUIRE_THAT(retrieved->alpha(), WithinAbs(0.5f, 0.001f));
    REQUIRE(retrieved->show_bounding_box == true);
    REQUIRE(retrieved->show_outline == true);
}

TEST_CASE("DisplayOptionsRegistry MaskDisplayOptions keys and count", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    MaskDisplayOptions opts;
    registry.set(QStringLiteral("mask_a"), opts);
    registry.set(QStringLiteral("mask_b"), opts);

    REQUIRE(registry.count<MaskDisplayOptions>() == 2);
    REQUIRE(registry.keys<MaskDisplayOptions>().size() == 2);
}

// ==================== Point Display Options Tests ====================

TEST_CASE("DisplayOptionsRegistry PointDisplayOptions set/get", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    PointDisplayOptions opts;
    opts.hex_color() = "#0000ff";
    opts.point_size = 10;
    opts.marker_shape = PointMarkerShape::Square;

    registry.set(QStringLiteral("point_1"), opts);

    auto const * retrieved = registry.get<PointDisplayOptions>(QStringLiteral("point_1"));
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->hex_color() == "#0000ff");
    REQUIRE(retrieved->point_size == 10);
    REQUIRE(retrieved->marker_shape == PointMarkerShape::Square);
}

// ==================== Tensor Display Options Tests ====================

TEST_CASE("DisplayOptionsRegistry TensorDisplayOptions set/get", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    TensorDisplayOptions opts;
    opts.hex_color() = "#ffff00";
    opts.display_channel = 2;

    registry.set(QStringLiteral("tensor_1"), opts);

    auto const * retrieved = registry.get<TensorDisplayOptions>(QStringLiteral("tensor_1"));
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->hex_color() == "#ffff00");
    REQUIRE(retrieved->display_channel == 2);
}

// ==================== Interval Display Options Tests ====================

TEST_CASE("DisplayOptionsRegistry DigitalIntervalDisplayOptions set/get", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    DigitalIntervalDisplayOptions opts;
    opts.hex_color() = "#ff00ff";
    opts.plotting_style = IntervalPlottingStyle::Border;
    opts.border_thickness = 10;
    opts.location = IntervalLocation::BottomLeft;

    registry.set(QStringLiteral("interval_1"), opts);

    auto const * retrieved = registry.get<DigitalIntervalDisplayOptions>(QStringLiteral("interval_1"));
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->hex_color() == "#ff00ff");
    REQUIRE(retrieved->plotting_style == IntervalPlottingStyle::Border);
    REQUIRE(retrieved->border_thickness == 10);
    REQUIRE(retrieved->location == IntervalLocation::BottomLeft);
}

// ==================== Media Display Options Tests ====================

TEST_CASE("DisplayOptionsRegistry MediaDisplayOptions set/get", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    MediaDisplayOptions opts;
    opts.hex_color() = "#00ffff";
    opts.contrast_options.active = true;
    opts.contrast_options.alpha = 1.5;
    opts.gamma_options.active = true;
    opts.gamma_options.gamma = 2.2;

    registry.set(QStringLiteral("media_1"), opts);

    auto const * retrieved = registry.get<MediaDisplayOptions>(QStringLiteral("media_1"));
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->hex_color() == "#00ffff");
    REQUIRE(retrieved->contrast_options.active == true);
    REQUIRE_THAT(retrieved->contrast_options.alpha, WithinAbs(1.5, 0.001));
    REQUIRE(retrieved->gamma_options.active == true);
    REQUIRE_THAT(retrieved->gamma_options.gamma, WithinAbs(2.2, 0.001));
}

// ==================== Visibility Tests ====================

TEST_CASE("DisplayOptionsRegistry setVisible/isVisible", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions line_opts;
    line_opts.is_visible() = false;
    registry.set(QStringLiteral("line_1"), line_opts);

    MaskDisplayOptions mask_opts;
    mask_opts.is_visible() = false;
    registry.set(QStringLiteral("mask_1"), mask_opts);

    // Check initial state
    REQUIRE_FALSE(registry.isVisible(QStringLiteral("line_1"), QStringLiteral("line")));
    REQUIRE_FALSE(registry.isVisible(QStringLiteral("mask_1"), QStringLiteral("mask")));

    // Set visible
    QSignalSpy visibility_spy(&registry, &DisplayOptionsRegistry::visibilityChanged);
    REQUIRE(visibility_spy.isValid());

    REQUIRE(registry.setVisible(QStringLiteral("line_1"), QStringLiteral("line"), true));
    REQUIRE(registry.isVisible(QStringLiteral("line_1"), QStringLiteral("line")));

    REQUIRE(visibility_spy.count() == 1);
    auto args = visibility_spy.takeFirst();
    REQUIRE(args.at(0).toString() == "line_1");
    REQUIRE(args.at(1).toString() == "line");
    REQUIRE(args.at(2).toBool() == true);

    // Setting same value doesn't emit signal
    REQUIRE(registry.setVisible(QStringLiteral("line_1"), QStringLiteral("line"), true));
    REQUIRE(visibility_spy.count() == 0);

    // Set invisible
    REQUIRE(registry.setVisible(QStringLiteral("line_1"), QStringLiteral("line"), false));
    REQUIRE_FALSE(registry.isVisible(QStringLiteral("line_1"), QStringLiteral("line")));
    REQUIRE(visibility_spy.count() == 1);
}

TEST_CASE("DisplayOptionsRegistry setVisible with non-existent key", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    REQUIRE_FALSE(registry.setVisible(QStringLiteral("nonexistent"), QStringLiteral("line"), true));
    REQUIRE_FALSE(registry.isVisible(QStringLiteral("nonexistent"), QStringLiteral("line")));
}

TEST_CASE("DisplayOptionsRegistry setVisible all types", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    // Create one of each type
    LineDisplayOptions line_opts;
    MaskDisplayOptions mask_opts;
    PointDisplayOptions point_opts;
    TensorDisplayOptions tensor_opts;
    DigitalIntervalDisplayOptions interval_opts;
    MediaDisplayOptions media_opts;

    registry.set(QStringLiteral("key"), line_opts);
    registry.set(QStringLiteral("key"), mask_opts);
    registry.set(QStringLiteral("key"), point_opts);
    registry.set(QStringLiteral("key"), tensor_opts);
    registry.set(QStringLiteral("key"), interval_opts);
    registry.set(QStringLiteral("key"), media_opts);

    // Test setVisible for each type
    REQUIRE(registry.setVisible(QStringLiteral("key"), QStringLiteral("line"), true));
    REQUIRE(registry.setVisible(QStringLiteral("key"), QStringLiteral("mask"), true));
    REQUIRE(registry.setVisible(QStringLiteral("key"), QStringLiteral("point"), true));
    REQUIRE(registry.setVisible(QStringLiteral("key"), QStringLiteral("tensor"), true));
    REQUIRE(registry.setVisible(QStringLiteral("key"), QStringLiteral("interval"), true));
    REQUIRE(registry.setVisible(QStringLiteral("key"), QStringLiteral("media"), true));

    REQUIRE(registry.isVisible(QStringLiteral("key"), QStringLiteral("line")));
    REQUIRE(registry.isVisible(QStringLiteral("key"), QStringLiteral("mask")));
    REQUIRE(registry.isVisible(QStringLiteral("key"), QStringLiteral("point")));
    REQUIRE(registry.isVisible(QStringLiteral("key"), QStringLiteral("tensor")));
    REQUIRE(registry.isVisible(QStringLiteral("key"), QStringLiteral("interval")));
    REQUIRE(registry.isVisible(QStringLiteral("key"), QStringLiteral("media")));
}

// ==================== Get Non-Existent Key Tests ====================

TEST_CASE("DisplayOptionsRegistry get returns nullptr for non-existent key", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    REQUIRE(registry.get<LineDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
    REQUIRE(registry.get<MaskDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
    REQUIRE(registry.get<PointDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
    REQUIRE(registry.get<TensorDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
    REQUIRE(registry.get<DigitalIntervalDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
    REQUIRE(registry.get<MediaDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
}

TEST_CASE("DisplayOptionsRegistry getMutable returns nullptr for non-existent key", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    REQUIRE(registry.getMutable<LineDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
    REQUIRE(registry.getMutable<MaskDisplayOptions>(QStringLiteral("nonexistent")) == nullptr);
}

// ==================== Multiple Types Same Key Tests ====================

TEST_CASE("DisplayOptionsRegistry different types can use same key", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions line_opts;
    line_opts.hex_color() = "#ff0000";

    MaskDisplayOptions mask_opts;
    mask_opts.hex_color() = "#00ff00";

    PointDisplayOptions point_opts;
    point_opts.hex_color() = "#0000ff";

    // All use the same key "data_1"
    registry.set(QStringLiteral("data_1"), line_opts);
    registry.set(QStringLiteral("data_1"), mask_opts);
    registry.set(QStringLiteral("data_1"), point_opts);

    // Each type should have its own options
    REQUIRE(registry.get<LineDisplayOptions>(QStringLiteral("data_1"))->hex_color() == "#ff0000");
    REQUIRE(registry.get<MaskDisplayOptions>(QStringLiteral("data_1"))->hex_color() == "#00ff00");
    REQUIRE(registry.get<PointDisplayOptions>(QStringLiteral("data_1"))->hex_color() == "#0000ff");

    // Counts should be independent
    REQUIRE(registry.count<LineDisplayOptions>() == 1);
    REQUIRE(registry.count<MaskDisplayOptions>() == 1);
    REQUIRE(registry.count<PointDisplayOptions>() == 1);
}

// ==================== Overwrite Tests ====================

TEST_CASE("DisplayOptionsRegistry set overwrites existing options", "[DisplayOptionsRegistry]")
{
    MediaWidgetStateData data;
    DisplayOptionsRegistry registry(&data);

    LineDisplayOptions opts1;
    opts1.hex_color() = "#ff0000";
    opts1.line_thickness = 2;

    registry.set(QStringLiteral("line_1"), opts1);

    LineDisplayOptions opts2;
    opts2.hex_color() = "#00ff00";
    opts2.line_thickness = 5;

    registry.set(QStringLiteral("line_1"), opts2);

    auto const * retrieved = registry.get<LineDisplayOptions>(QStringLiteral("line_1"));
    REQUIRE(retrieved->hex_color() == "#00ff00");
    REQUIRE(retrieved->line_thickness == 5);

    // Count should still be 1
    REQUIRE(registry.count<LineDisplayOptions>() == 1);
}
