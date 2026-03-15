/**
 * @file ProcessingOptionsSchema.hpp
 * @brief ParameterUIHints specializations for ImageProcessing option structs
 *
 * This companion header provides reflect-cpp UI annotations for the plain
 * structs defined in ImageProcessing/ProcessingOptions.hpp. Keeping these
 * hints separate avoids adding an rfl dependency to the ImageProcessing library.
 */

#ifndef MEDIA_PROCESSING_PIPELINE_PROCESSING_OPTIONS_SCHEMA_HPP
#define MEDIA_PROCESSING_PIPELINE_PROCESSING_OPTIONS_SCHEMA_HPP

#include "ImageProcessing/ProcessingOptions.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

namespace WhiskerToolbox::Transforms::V2 {

template<>
struct ParameterUIHints<ContrastOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("alpha")) {
            f->tooltip = "Contrast multiplier (1.0 = no change)";
            f->min_value = 0.0;
            f->max_value = 10.0;
        }
        if (auto * f = schema.field("beta")) {
            f->tooltip = "Brightness offset added to each pixel";
            f->min_value = -255;
            f->max_value = 255;
        }
        if (auto * f = schema.field("display_min")) {
            f->tooltip = "Minimum display value (mapped to 0)";
            f->min_value = 0.0;
            f->max_value = 65535.0;
        }
        if (auto * f = schema.field("display_max")) {
            f->tooltip = "Maximum display value (mapped to 255)";
            f->min_value = 0.0;
            f->max_value = 65535.0;
        }
    }
};

template<>
struct ParameterUIHints<GammaOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("gamma")) {
            f->tooltip = "Gamma correction value (1.0 = no change, <1 = brighter, >1 = darker)";
            f->min_value = 0.01;
            f->max_value = 10.0;
            f->is_exclusive_min = true;
        }
    }
};

template<>
struct ParameterUIHints<SharpenOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("sigma")) {
            f->tooltip = "Sigma parameter for Gaussian sharpening kernel";
            f->min_value = 0.1;
            f->max_value = 20.0;
            f->is_exclusive_min = true;
        }
    }
};

template<>
struct ParameterUIHints<ClaheOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("grid_size")) {
            f->tooltip = "Tile grid size for CLAHE (larger = more local contrast)";
            f->min_value = 1;
            f->max_value = 64;
        }
        if (auto * f = schema.field("clip_limit")) {
            f->tooltip = "Contrast limit for CLAHE (higher = more contrast enhancement)";
            f->min_value = 0.1;
            f->max_value = 100.0;
            f->is_exclusive_min = true;
        }
    }
};

template<>
struct ParameterUIHints<BilateralOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("diameter")) {
            f->tooltip = "Diameter of pixel neighborhood for filtering";
            f->min_value = 1;
            f->max_value = 25;
        }
        if (auto * f = schema.field("sigma_color")) {
            f->tooltip = "Filter sigma in the color space (higher = wider color range mixed)";
            f->min_value = 1.0;
            f->max_value = 200.0;
        }
        if (auto * f = schema.field("sigma_spatial")) {
            f->tooltip = "Filter sigma in the coordinate space (higher = larger area of influence)";
            f->min_value = 1.0;
            f->max_value = 200.0;
        }
    }
};

template<>
struct ParameterUIHints<MedianOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("kernel_size")) {
            f->tooltip = "Kernel size for median filter (must be odd, >= 3)";
            f->min_value = 3;
            f->max_value = 255;
        }
    }
};

template<>
struct ParameterUIHints<MagicEraserParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("brush_size")) {
            f->tooltip = "Diameter of the eraser brush in pixels";
            f->min_value = 1;
            f->max_value = 100;
        }
        if (auto * f = schema.field("median_filter_size")) {
            f->tooltip = "Median filter kernel size for erased region (must be odd)";
            f->min_value = 3;
            f->max_value = 101;
        }
    }
};

template<>
struct ParameterUIHints<ColormapOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("colormap")) {
            f->tooltip = "Colormap to apply to grayscale images";
        }
        if (auto * f = schema.field("alpha")) {
            f->tooltip = "Alpha blending with original image (0.0 = original, 1.0 = full colormap)";
            f->min_value = 0.0;
            f->max_value = 1.0;
        }
        if (auto * f = schema.field("normalize")) {
            f->tooltip = "Normalize image values to full range before applying colormap";
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// MEDIA_PROCESSING_PIPELINE_PROCESSING_OPTIONS_SCHEMA_HPP
