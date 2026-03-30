#ifndef MASK_DATA_LOADER_HPP
#define MASK_DATA_LOADER_HPP

#include "ParameterSchema/ParameterSchema.hpp"

#include <memory>
#include <string>

class MaskData;

struct HDF5MaskLoaderOptions {
    std::string filename;
    std::string frame_key = "frames";
    std::string x_key = "widths";
    std::string y_key = "heights";
};

std::shared_ptr<MaskData> load(HDF5MaskLoaderOptions & opts);

template<>
struct ParameterUIHints<HDF5MaskLoaderOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (import UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Path to the HDF5 file containing frame indices and per-frame mask coordinate datasets";
        }
        if (auto * f = schema.field("frame_key")) {
            f->tooltip = "Dataset name for frame indices aligned with each mask (default: frames)";
        }
        if (auto * f = schema.field("x_key")) {
            f->tooltip = "Dataset name for ragged x pixel coordinates per frame (default dataset name: widths)";
        }
        if (auto * f = schema.field("y_key")) {
            f->tooltip = "Dataset name for ragged y pixel coordinates per frame (default dataset name: heights)";
        }
    }
};

#endif// MASK_DATA_LOADER_HPP
