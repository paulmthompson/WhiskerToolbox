#ifndef LINE_DATA_BINARY_HPP
#define LINE_DATA_BINARY_HPP

#include <memory>// For std::shared_ptr
#include <string>


class LineData;

class BinaryFileCapnpStorage {
public:
    BinaryFileCapnpStorage() = default;

    bool save(LineData const & data, std::string const & file_path);

    std::shared_ptr<LineData> load(std::string const & file_path);

    //std::shared_ptr<LineData> loadMemoryMapped(const void* mapped_data, size_t mapped_size);
};

#endif// LINE_DATA_BINARY_HPP
