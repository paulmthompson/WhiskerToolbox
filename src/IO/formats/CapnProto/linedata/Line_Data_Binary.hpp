#ifndef LINE_DATA_BINARY_HPP
#define LINE_DATA_BINARY_HPP

#include "ParameterSchema/ParameterSchema.hpp"

#include <memory>// For std::shared_ptr
#include <string>


class LineData;

struct BinaryLineLoaderOptions {
    std::string file_path;
};

std::shared_ptr<LineData> load(BinaryLineLoaderOptions & opts);

struct BinaryLineSaverOptions {
    std::string filename;
    std::string parent_dir = ".";
};

bool save(LineData const & data, BinaryLineSaverOptions const & opts);

namespace WhiskerToolbox::Transforms::V2 {

template<>
struct ParameterUIHints<BinaryLineSaverOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Output filename for the binary CapnProto file";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory in which to create the output file";
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// LINE_DATA_BINARY_HPP
