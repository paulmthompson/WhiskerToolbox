#ifndef LINE_DATA_BINARY_HPP
#define LINE_DATA_BINARY_HPP

#include "datamanagerio_capnproto_export.h"

#include "ParameterSchema/ParameterSchema.hpp"

#include <memory>// For std::shared_ptr
#include <string>


class LineData;

struct BinaryLineLoaderOptions {
    std::string file_path;
};

DATAMANAGERIO_CAPNPROTO_EXPORT std::shared_ptr<LineData> load(BinaryLineLoaderOptions & opts);

template<>
struct ParameterUIHints<BinaryLineLoaderOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (import UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("file_path")) {
            f->tooltip = "Path to the CapnProto-encoded binary line data file";
        }
    }
};

struct BinaryLineSaverOptions {
    std::string filename;
    std::string parent_dir = ".";
};

DATAMANAGERIO_CAPNPROTO_EXPORT bool save(LineData const & data, BinaryLineSaverOptions const & opts);


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

#endif// LINE_DATA_BINARY_HPP
