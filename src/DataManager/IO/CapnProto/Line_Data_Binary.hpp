#ifndef LINE_DATA_BINARY_HPP
#define LINE_DATA_BINARY_HPP

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

bool save(LineData const & data, BinaryLineSaverOptions & opts);


#endif// LINE_DATA_BINARY_HPP
