#include "SVGExporter.hpp"

#include "Core/DataViewerState.hpp"
#include "Core/DataViewerStateData.hpp"
#include "OpenGLWidget.hpp"
#include "SceneBuildingHelpers.hpp"
#include "TransformComposers.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"
#include "CorePlotting/Export/SVGPrimitives.hpp"
#include "DataManager/utils/color.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

SVGExporter::SVGExporter(OpenGLWidget * gl_widget)
    : gl_widget_(gl_widget) {
}

QString SVGExporter::exportToSVG() {
    // Get current visible time window from OpenGL widget view state
    auto const & view_state = gl_widget_->getViewState();
    auto const start_time = view_state.time_start;
    auto const end_time = view_state.time_end;

    std::cout << "SVG Export - Time range: "
              << start_time << " to "
              << end_time << std::endl;
    std::cout << "SVG Export - Y range: "
              << view_state.y_min
              << " to " << view_state.y_max
              << std::endl;

    // Build scene from current plot state
    CorePlotting::RenderableScene scene = buildScene(start_time, end_time);

    // Set up SVG export parameters
    CorePlotting::SVGExportParams params;
    params.canvas_width = svg_width_;
    params.canvas_height = svg_height_;
    params.background_color = gl_widget_->getBackgroundColor();

    // Render scene to SVG
    std::string svg_content = CorePlotting::buildSVGDocument(scene, params);

    // If scalebar is enabled, we need to add it to the document
    if (scalebar_enabled_) {
        // Insert scalebar elements before the closing </svg> tag
        auto scalebar_elements = CorePlotting::createScalebarSVG(
                scalebar_length_,
                static_cast<float>(start_time),
                static_cast<float>(end_time),
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

CorePlotting::RenderableScene SVGExporter::buildScene(int start_time, int end_time) const {
    CorePlotting::RenderableScene scene;

    auto const view_state = gl_widget_->getViewState();
    auto const * state = gl_widget_->state();

    auto const y_min = view_state.y_min;
    auto const y_max = view_state.y_max;

    // Build shared View and Projection matrices
    // Get view state parameters from OpenGLWidget
    CorePlotting::ViewProjectionParams view_params;
    view_params.vertical_pan_offset = view_state.vertical_pan_offset;

    scene.view_matrix = CorePlotting::getAnalogViewMatrix(view_params);
    scene.projection_matrix = CorePlotting::getAnalogProjectionMatrix(
            TimeFrameIndex(start_time), TimeFrameIndex(end_time), y_min, y_max);

    // 1. Build interval batches (rendered as background)
    auto const & interval_series_map = gl_widget_->getDigitalIntervalSeriesMap();
    for (auto const & [key, interval_data]: interval_series_map) {
        auto const * opts = state->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            auto batch = buildIntervalBatch(
                    interval_data.series,
                    interval_data.layout_transform,
                    *opts,
                    static_cast<float>(start_time),
                    static_cast<float>(end_time));
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
                    start_time,
                    end_time);
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
                    start_time,
                    end_time);
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
        int start_time,
        int end_time) const {

    auto const view_state = gl_widget_->getViewState();

    // Create layout from layout_transform
    CorePlotting::SeriesLayout layout{
            "",// key not needed for matrix generation
            layout_transform,
            0};

    // Compose Y transform using the new LayoutTransform-based pattern
    CorePlotting::LayoutTransform y_transform = DataViewer::composeAnalogYTransform(
            layout,
            data_cache.cached_mean,
            data_cache.cached_std_dev,
            data_cache.intrinsic_scale,
            options.user_scale_factor,
            options.y_offset,
            view_state.global_zoom,
            view_state.global_vertical_scale);

    // Create model matrix from composed transform
    glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

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
    batch_params.start_time = TimeFrameIndex(start_time);
    batch_params.end_time = TimeFrameIndex(end_time);
    batch_params.color = color;
    batch_params.thickness = static_cast<float>(options.get_line_thickness());
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
        int start_time,
        int end_time) const {

    auto const view_state = gl_widget_->getViewState();

    auto const y_min = view_state.y_min;
    auto const y_max = view_state.y_max;

    // Create layout from layout_transform
    CorePlotting::SeriesLayout layout{
            "",
            layout_transform,
            0};

    // Compose Y transform based on plotting mode
    CorePlotting::LayoutTransform y_transform;
    if (options.plotting_mode == EventPlottingModeData::FullCanvas) {
        y_transform = DataViewer::composeEventFullCanvasYTransform(y_min, y_max, options.margin_factor);
    } else {
        y_transform = DataViewer::composeEventYTransform(layout, options.margin_factor, view_state.global_vertical_scale);
    }

    // Create model matrix from composed transform
    glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

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
    batch_params.start_time = TimeFrameIndex(start_time);
    batch_params.end_time = TimeFrameIndex(end_time);
    batch_params.color = color;
    batch_params.glyph_size = static_cast<float>(options.get_line_thickness());
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
        float start_time,
        float end_time) const {

    auto const view_state = gl_widget_->getViewState();

    // Create layout from layout_transform
    CorePlotting::SeriesLayout layout{
            "",
            layout_transform,
            0};

    // Compose Y transform for intervals
    CorePlotting::LayoutTransform y_transform = DataViewer::composeIntervalYTransform(
            layout,
            options.margin_factor,
            view_state.global_zoom,
            view_state.global_vertical_scale);

    // Create model matrix from composed transform
    glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

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
    batch_params.start_time = TimeFrameIndex(static_cast<int64_t>(start_time));
    batch_params.end_time = TimeFrameIndex(static_cast<int64_t>(end_time));
    batch_params.color = color;

    // Use simplified API (takes pre-composed model matrix)
    return DataViewerHelpers::buildIntervalSeriesBatchSimplified(
            *series,
            gl_widget_->getMasterTimeFrame(),
            batch_params,
            model_matrix);
}
