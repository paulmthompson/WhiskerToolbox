#include "SVGExporter.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"
#include "CorePlotting/Export/SVGPrimitives.hpp"
#include "DataManager/utils/color.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "OpenGLWidget.hpp"
#include "SceneBuildingHelpers.hpp"
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

    std::cout << "SVG Export - Time range: " << start_time << " to " << end_time << std::endl;
    std::cout << "SVG Export - Y range: " << gl_widget_->getYMin() << " to " << gl_widget_->getYMax() << std::endl;

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

    auto const y_min = gl_widget_->getYMin();
    auto const y_max = gl_widget_->getYMax();

    // Build shared View and Projection matrices
    // Get view state parameters from OpenGLWidget (Phase 4.7+ migration)
    CorePlotting::ViewProjectionParams view_params;
    view_params.vertical_pan_offset = gl_widget_->getVerticalPanOffset();

    scene.view_matrix = CorePlotting::getAnalogViewMatrix(view_params);
    scene.projection_matrix = CorePlotting::getAnalogProjectionMatrix(
            TimeFrameIndex(start_time), TimeFrameIndex(end_time), y_min, y_max);

    // 1. Build interval batches (rendered as background)
    auto const & interval_series_map = gl_widget_->getDigitalIntervalSeriesMap();
    for (auto const & [key, interval_data]: interval_series_map) {
        if (interval_data.display_options->style.is_visible) {
            auto batch = buildIntervalBatch(
                    interval_data.series,
                    *interval_data.display_options,
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
        if (analog_data.display_options->style.is_visible) {
            auto batch = buildAnalogBatch(
                    analog_data.series,
                    *analog_data.display_options,
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
        if (event_data.display_options->style.is_visible) {
            auto batch = buildEventBatch(
                    event_data.series,
                    *event_data.display_options,
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
        NewAnalogTimeSeriesDisplayOptions const & display_options,
        int start_time,
        int end_time) const {

    // Build model params from display options
    CorePlotting::AnalogSeriesMatrixParams model_params;
    model_params.allocated_y_center = display_options.layout.allocated_y_center;
    model_params.allocated_height = display_options.layout.allocated_height;
    model_params.intrinsic_scale = display_options.scaling.intrinsic_scale;
    model_params.user_scale_factor = display_options.user_scale_factor;
    model_params.global_zoom = gl_widget_->getGlobalZoom();
    model_params.user_vertical_offset = display_options.scaling.user_vertical_offset;
    model_params.data_mean = display_options.data_cache.cached_mean;
    model_params.std_dev = display_options.data_cache.cached_std_dev;
    model_params.global_vertical_scale = gl_widget_->getGlobalVerticalScale();

    CorePlotting::ViewProjectionParams view_params;
    view_params.vertical_pan_offset = gl_widget_->getVerticalPanOffset();

    // Convert hex color to glm::vec4
    int r, g, b;
    hexToRGB(display_options.style.hex_color, r, g, b);
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
    batch_params.thickness = display_options.style.line_thickness;
    batch_params.detect_gaps = (display_options.gap_handling == AnalogGapHandling::DetectGaps);
    batch_params.gap_threshold = display_options.gap_threshold;

    // Use SceneBuildingHelpers to build the batch
    return DataViewerHelpers::buildAnalogSeriesBatch(
            *series,
            gl_widget_->getMasterTimeFrame(),
            batch_params,
            model_params,
            view_params);
}

CorePlotting::RenderableGlyphBatch SVGExporter::buildEventBatch(
        std::shared_ptr<DigitalEventSeries> const & series,
        NewDigitalEventSeriesDisplayOptions const & display_options,
        int start_time,
        int end_time) const {

    // Build model params from display options
    CorePlotting::EventSeriesMatrixParams model_params;
    model_params.allocated_y_center = display_options.layout.allocated_y_center;
    model_params.allocated_height = display_options.layout.allocated_height;
    model_params.event_height = display_options.event_height;
    model_params.margin_factor = display_options.margin_factor;
    model_params.global_vertical_scale = gl_widget_->getGlobalVerticalScale();
    model_params.viewport_y_min = gl_widget_->getYMin();
    model_params.viewport_y_max = gl_widget_->getYMax();
    model_params.plotting_mode = (display_options.plotting_mode == EventPlottingMode::FullCanvas)
                                         ? CorePlotting::EventSeriesMatrixParams::PlottingMode::FullCanvas
                                         : CorePlotting::EventSeriesMatrixParams::PlottingMode::Stacked;

    CorePlotting::ViewProjectionParams view_params;
    view_params.vertical_pan_offset = gl_widget_->getVerticalPanOffset();

    // Convert hex color to glm::vec4
    int r, g, b;
    hexToRGB(display_options.style.hex_color, r, g, b);
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
    batch_params.glyph_size = display_options.style.line_thickness;
    batch_params.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Tick;

    // Build the batch
    auto batch = DataViewerHelpers::buildEventSeriesBatch(
            *series,
            gl_widget_->getMasterTimeFrame(),
            batch_params,
            model_params,
            view_params);

    // Set colors for all events (SceneBuildingHelpers doesn't set per-glyph colors)
    batch.colors.resize(batch.positions.size(), color);

    return batch;
}

CorePlotting::RenderableRectangleBatch SVGExporter::buildIntervalBatch(
        std::shared_ptr<DigitalIntervalSeries> const & series,
        NewDigitalIntervalSeriesDisplayOptions const & display_options,
        float start_time,
        float end_time) const {

    // Build model params from display options
    CorePlotting::IntervalSeriesMatrixParams model_params;
    model_params.allocated_y_center = display_options.layout.allocated_y_center;
    model_params.allocated_height = display_options.layout.allocated_height;
    model_params.margin_factor = display_options.margin_factor;
    model_params.global_zoom = gl_widget_->getGlobalZoom();
    model_params.global_vertical_scale = gl_widget_->getGlobalVerticalScale();
    model_params.extend_full_canvas = display_options.extend_full_canvas;

    CorePlotting::ViewProjectionParams view_params;
    view_params.vertical_pan_offset = gl_widget_->getVerticalPanOffset();

    // Convert hex color to glm::vec4 with alpha
    int r, g, b;
    hexToRGB(display_options.style.hex_color, r, g, b);
    glm::vec4 const color(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            display_options.style.alpha);

    // Set up batch params
    DataViewerHelpers::IntervalBatchParams batch_params;
    batch_params.start_time = TimeFrameIndex(static_cast<int64_t>(start_time));
    batch_params.end_time = TimeFrameIndex(static_cast<int64_t>(end_time));
    batch_params.color = color;

    // Use SceneBuildingHelpers to build the batch
    return DataViewerHelpers::buildIntervalSeriesBatch(
            *series,
            gl_widget_->getMasterTimeFrame(),
            batch_params,
            model_params,
            view_params);
}
