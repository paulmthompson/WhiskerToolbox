#include "SVGExporter.hpp"

#include "Core/DataViewerState.hpp"
#include "Core/DataViewerStateData.hpp"
#include "OpenGLWidget.hpp"
#include "SceneBuildingHelpers.hpp"
#include "TransformComposers.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CoreUtilities/color.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "PlottingSVG/SVGExport.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>

namespace {

[[nodiscard]] int64_t masterAbsoluteTimeAtIndex(
        std::shared_ptr<TimeFrame> const & master_tf,
        TimeFrameIndex const idx) {
    if (!master_tf) {
        return 0;
    }
    return static_cast<int64_t>(master_tf->getTimeAtIndex(idx));
}

[[nodiscard]] int64_t masterAbsoluteTimeSpanForOrtho(
        std::shared_ptr<TimeFrame> const & master_tf,
        TimeFrameIndex const start,
        TimeFrameIndex const end) {
    if (!master_tf) {
        return std::max<int64_t>(end.getValue() - start.getValue(), 1);
    }
    int64_t const t0 = static_cast<int64_t>(master_tf->getTimeAtIndex(start));
    int64_t const t1 = static_cast<int64_t>(master_tf->getTimeAtIndex(end));
    return std::max<int64_t>(t1 - t0, 1);
}

}// namespace

SVGExporter::SVGExporter(OpenGLWidget * gl_widget)
    : gl_widget_(gl_widget) {
}

QString SVGExporter::exportToSVG() {
    // Get current visible time window from OpenGL widget view state
    auto const & view_state = gl_widget_->getViewState();
    auto const master_window_start = TimeFrameIndex{static_cast<int64_t>(view_state.x_min)};
    auto const master_window_end = TimeFrameIndex{static_cast<int64_t>(view_state.x_max)};

    std::cout << "SVG Export - Time range: "
              << master_window_start.getValue() << " to "
              << master_window_end.getValue()
              << std::endl;
    std::cout << "SVG Export - Y range: "
              << view_state.y_min
              << " to " << view_state.y_max
              << std::endl;

    // Build scene from current plot state
    CorePlotting::RenderableScene const scene = buildScene(master_window_start, master_window_end);

    // Set up SVG export parameters
    PlottingSVG::SVGExportParams params;
    params.canvas_width = svg_width_;
    params.canvas_height = svg_height_;
    params.background_color = gl_widget_->getBackgroundColor();

    // Render scene to SVG
    std::string svg_content = PlottingSVG::buildSVGDocument(scene, params);

    // If scalebar is enabled, we need to add it to the document
    if (scalebar_enabled_) {
        // Insert scalebar elements before the closing </svg> tag
        auto scalebar_elements = PlottingSVG::createScalebarSVG(
                scalebar_length_,
                PlottingSVG::ScalebarTimeRange{
                        static_cast<float>(master_window_start.getValue()),
                        static_cast<float>(master_window_end.getValue())},
                params);

        // Find position to insert (before </svg>)
        size_t const close_tag_pos = svg_content.rfind("</svg>");
        if (close_tag_pos != std::string::npos) {
            std::string scalebar_content;
            for (auto const & elem: scalebar_elements) {
                scalebar_content += "  " + elem + "\n";
            }
            svg_content.insert(close_tag_pos, scalebar_content);
        }
    }

    return QString::fromStdString(svg_content);
}

CorePlotting::RenderableScene SVGExporter::buildScene(
        TimeFrameIndex const master_window_start,
        TimeFrameIndex const master_window_end) const {
    CorePlotting::RenderableScene scene;

    auto const view_state = gl_widget_->getViewState();
    auto const * state = gl_widget_->state();
    auto const mtf = gl_widget_->getMasterTimeFrame();
    int64_t const span = masterAbsoluteTimeSpanForOrtho(mtf, master_window_start, master_window_end);
    auto const local_end_time = TimeFrameIndex(span);

    // Fold y_zoom and y_pan into projection via effective viewport
    auto const eff = CorePlotting::computeEffectiveYViewport(view_state);

    // View matrix is identity — pan and zoom fully handled by projection
    scene.view_matrix = glm::mat4(1.0f);
    scene.projection_matrix = CorePlotting::getAnalogProjectionMatrix(
            TimeFrameIndex{0}, local_end_time, eff.y_min, eff.y_max);
    scene.time_axis_origin_master_absolute = masterAbsoluteTimeAtIndex(mtf, master_window_start);

    // 1. Build interval batches (rendered as background)
    auto const & interval_series_map = gl_widget_->getDigitalIntervalSeriesMap();
    for (auto const & [key, interval_data]: interval_series_map) {
        auto const * opts = state->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            auto batch = buildIntervalBatch(
                    interval_data.series,
                    interval_data.layout_transform,
                    *opts,
                    master_window_start,
                    master_window_end);
            if (!batch.bounds.empty()) {
                scene.rectangle_batches.push_back(std::move(batch));
            }
        }
    }

    // 2. Build analog series batches
    auto const & analog_series_map = gl_widget_->getAnalogSeriesMap();
    for (auto const & [key, analog_data]: analog_series_map) {
        auto const * opts = state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            auto batch = buildAnalogBatch(
                    analog_data.series,
                    analog_data.layout_transform,
                    analog_data.data_cache,
                    *opts,
                    master_window_start,
                    master_window_end);
            if (!batch.vertices.empty()) {
                scene.poly_line_batches.push_back(std::move(batch));
            }
        }
    }

    // 3. Build event series batches
    auto const & event_series_map = gl_widget_->getDigitalEventSeriesMap();
    for (auto const & [key, event_data]: event_series_map) {
        auto const * opts = state->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            auto batch = buildEventBatch(
                    event_data.series,
                    event_data.layout_transform,
                    *opts,
                    master_window_start,
                    master_window_end);
            if (!batch.positions.empty()) {
                scene.glyph_batches.push_back(std::move(batch));
            }
        }
    }

    std::cout << "SVG Export - Built scene with "
              << scene.rectangle_batches.size() << " interval batches, "
              << scene.poly_line_batches.size() << " polyline batches, "
              << scene.glyph_batches.size() << " glyph batches" << std::endl;

    return scene;
}

CorePlotting::RenderablePolyLineBatch SVGExporter::buildAnalogBatch(
        std::shared_ptr<AnalogTimeSeries> const & series,
        CorePlotting::LayoutTransform const & layout_transform,
        CorePlotting::SeriesDataCache const & data_cache,
        AnalogSeriesOptionsData const & options,
        TimeFrameIndex const master_window_start,
        TimeFrameIndex const master_window_end) const {

    // Create layout from layout_transform
    CorePlotting::SeriesLayout const layout{
            "",// key not needed for matrix generation
            layout_transform,
            0};

    // Compose Y transform using the new LayoutTransform-based pattern
    CorePlotting::LayoutTransform const y_transform = DataViewer::composeAnalogYTransform(
            layout,
            data_cache.cached_mean,
            data_cache.cached_std_dev,
            data_cache.intrinsic_scale,
            options.user_scale_factor,
            options.y_offset,
            gl_widget_->state()->globalYScale(),
            gl_widget_->state()->marginFactor());

    // Create model matrix from composed transform
    glm::mat4 const model_matrix = CorePlotting::createModelMatrix(y_transform);

    // Convert hex color to glm::vec4
    int r, g, b;
    hexToRGB(options.hex_color(), r, g, b);
    glm::vec4 const color(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            1.0f);

    // Set up batch params
    DataViewerHelpers::AnalogBatchParams batch_params;
    batch_params.start_time = master_window_start;
    batch_params.end_time = master_window_end;
    if (auto const mtf = gl_widget_->getMasterTimeFrame()) {
        batch_params.x_origin_master_absolute_time = masterAbsoluteTimeAtIndex(mtf, master_window_start);
    }
    batch_params.color = color;
    batch_params.thickness = options.get_line_thickness();
    batch_params.detect_gaps = (options.gap_handling == AnalogGapHandlingMode::DetectGaps);
    batch_params.gap_threshold = options.gap_threshold;

    // Use simplified API (takes pre-composed model matrix)
    return DataViewerHelpers::buildAnalogSeriesBatchSimplified(
            *series,
            gl_widget_->getMasterTimeFrame(),
            batch_params,
            model_matrix);
}

CorePlotting::RenderableGlyphBatch SVGExporter::buildEventBatch(
        std::shared_ptr<DigitalEventSeries> const & series,
        CorePlotting::LayoutTransform const & layout_transform,
        DigitalEventSeriesOptionsData const & options,
        TimeFrameIndex const master_window_start,
        TimeFrameIndex const master_window_end) const {

    auto const view_state = gl_widget_->getViewState();

    auto const y_min = static_cast<float>(view_state.y_min);
    auto const y_max = static_cast<float>(view_state.y_max);

    // Create layout from layout_transform
    CorePlotting::SeriesLayout const layout{
            "",
            layout_transform,
            0};

    // Compose Y transform based on plotting mode
    CorePlotting::LayoutTransform y_transform;
    float lane_half_height = 0.0f;
    if (options.plotting_mode == EventPlottingModeData::FullCanvas) {
        y_transform = DataViewer::composeEventFullCanvasYTransform(y_min, y_max, options.margin_factor);
        lane_half_height = (y_max - y_min) * 0.5f;
    } else {
        y_transform = DataViewer::composeEventYTransform(layout, options.margin_factor, gl_widget_->state()->globalYScale());
        lane_half_height = layout.y_transform.gain;
    }

    // Create model matrix from composed transform
    glm::mat4 const model_matrix = CorePlotting::createModelMatrix(y_transform);

    // Convert hex color to glm::vec4
    int r, g, b;
    hexToRGB(options.hex_color(), r, g, b);
    glm::vec4 const color(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            1.0f);

    // Set up batch params
    DataViewerHelpers::EventBatchParams batch_params;
    batch_params.start_time = master_window_start;
    batch_params.end_time = master_window_end;
    if (auto const mtf = gl_widget_->getMasterTimeFrame()) {
        batch_params.x_origin_master_absolute_time = masterAbsoluteTimeAtIndex(mtf, master_window_start);
    }
    batch_params.color = color;
    batch_params.glyph_size = DataViewer::computeEventGlyphSize(
            y_transform,
            lane_half_height,
            options.event_height,
            options.margin_factor,
            static_cast<float>(options.get_line_thickness()));
    batch_params.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Tick;

    // Use simplified API (takes pre-composed model matrix)
    auto batch = DataViewerHelpers::buildEventSeriesBatchSimplified(
            *series,
            gl_widget_->getMasterTimeFrame(),
            batch_params,
            model_matrix);

    // Set colors for all events (SceneBuildingHelpers doesn't set per-glyph colors)
    batch.colors.resize(batch.positions.size(), color);

    return batch;
}

CorePlotting::RenderableRectangleBatch SVGExporter::buildIntervalBatch(
        std::shared_ptr<DigitalIntervalSeries> const & series,
        CorePlotting::LayoutTransform const & layout_transform,
        DigitalIntervalSeriesOptionsData const & options,
        TimeFrameIndex const master_window_start,
        TimeFrameIndex const master_window_end) const {

    // Create layout from layout_transform
    CorePlotting::SeriesLayout const layout{
            "",
            layout_transform,
            0};

    // Compose Y transform for intervals
    CorePlotting::LayoutTransform const y_transform = DataViewer::composeIntervalYTransform(
            layout,
            options.margin_factor,
            gl_widget_->state()->globalYScale());

    // Create model matrix from composed transform
    glm::mat4 const model_matrix = CorePlotting::createModelMatrix(y_transform);

    // Convert hex color to glm::vec4 with alpha
    int r, g, b;
    hexToRGB(options.hex_color(), r, g, b);
    glm::vec4 const color(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            options.get_alpha());

    // Set up batch params
    DataViewerHelpers::IntervalBatchParams batch_params;
    batch_params.start_time = master_window_start;
    batch_params.end_time = master_window_end;
    if (auto const mtf = gl_widget_->getMasterTimeFrame()) {
        batch_params.x_origin_master_absolute_time = masterAbsoluteTimeAtIndex(mtf, master_window_start);
    }
    batch_params.color = color;

    // Use simplified API (takes pre-composed model matrix)
    return DataViewerHelpers::buildIntervalSeriesBatchSimplified(
            *series,
            gl_widget_->getMasterTimeFrame(),
            batch_params,
            model_matrix);
}
