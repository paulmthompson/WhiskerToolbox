/**
 * @file Mask_Data_CSV.cpp
 * @brief Implementation of CSV RLE loader/saver for MaskData
 */
#include "Mask_Data_CSV.hpp"

#include "IO/core/AtomicWrite.hpp"
#include "Masks/Mask_Data.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

std::string encode_mask_rle(Mask2D const & mask, std::string const & rle_delimiter) {
    if (mask.empty()) {
        return {};
    }

    // Copy and sort points by (y, x) for run detection
    auto points = mask.points();
    std::sort(points.begin(), points.end(), [](auto const & a, auto const & b) {
        return (a.y < b.y) || (a.y == b.y && a.x < b.x);
    });

    std::ostringstream oss;
    bool first = true;

    size_t i = 0;
    while (i < points.size()) {
        uint32_t const y = points[i].y;
        uint32_t const x_start = points[i].x;
        uint32_t run_length = 1;

        // Extend the run while consecutive x values at the same y
        while (i + run_length < points.size() &&
               points[i + run_length].y == y &&
               points[i + run_length].x == x_start + run_length) {
            ++run_length;
        }

        if (!first) {
            oss << rle_delimiter;
        }
        oss << y << rle_delimiter << x_start << rle_delimiter << run_length;
        first = false;

        i += run_length;
    }

    return oss.str();
}

Mask2D decode_mask_rle(std::string const & rle_str, std::string const & rle_delimiter) {
    if (rle_str.empty()) {
        return {};
    }

    std::vector<Point2D<uint32_t>> points;

    // Parse triplets: y, x_start, length, y, x_start, length, ...
    char const delim_char = rle_delimiter.empty() ? ',' : rle_delimiter[0];
    char const * start = rle_str.c_str();
    char const * end = start + rle_str.length();
    char * parse_end = nullptr;

    while (start < end) {
        // Parse y
        auto y_val = std::strtoul(start, &parse_end, 10);
        if (parse_end == start) { break; }
        start = parse_end;
        if (start < end && *start == delim_char) { ++start; }

        // Parse x_start
        auto x_start_val = std::strtoul(start, &parse_end, 10);
        if (parse_end == start) { break; }
        start = parse_end;
        if (start < end && *start == delim_char) { ++start; }

        // Parse length
        auto length_val = std::strtoul(start, &parse_end, 10);
        if (parse_end == start) { break; }
        start = parse_end;
        if (start < end && *start == delim_char) { ++start; }

        // Expand run into individual pixels
        auto y = static_cast<uint32_t>(y_val);
        auto x_s = static_cast<uint32_t>(x_start_val);
        auto len = static_cast<uint32_t>(length_val);
        points.reserve(points.size() + len);
        for (uint32_t dx = 0; dx < len; ++dx) {
            points.emplace_back(x_s + dx, y);
        }
    }

    return Mask2D(std::move(points));
}

std::map<TimeFrameIndex, std::vector<Mask2D>> load(CSVMaskRLELoaderOptions const & opts) {
    std::map<TimeFrameIndex, std::vector<Mask2D>> data_map;

    std::ifstream file(opts.filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + opts.filepath);
    }

    std::string line;
    line.reserve(4096);
    int loaded_masks = 0;
    bool is_first_line = true;

    std::string const delimiter = opts.getDelimiter();
    std::string const rle_delimiter = opts.getRLEDelimiter();
    bool const has_header = opts.getHasHeader();
    std::string const header_identifier = opts.getHeaderIdentifier();

    while (std::getline(file, line)) {
        // Skip header if present
        if (is_first_line && has_header) {
            is_first_line = false;
            if (line.find(header_identifier) != std::string::npos) {
                continue;
            }
        }
        is_first_line = false;

        // Parse: Frame,"y,x_start,len,y,x_start,len,..."
        size_t const comma_pos = line.find(delimiter[0]);
        if (comma_pos == std::string::npos) {
            continue;
        }

        int const frame_num = std::stoi(line.substr(0, comma_pos));

        // Find quoted RLE data
        size_t const quote1 = line.find('"', comma_pos + 1);
        if (quote1 == std::string::npos) {
            continue;
        }
        size_t const quote2 = line.find('"', quote1 + 1);
        if (quote2 == std::string::npos) {
            continue;
        }

        std::string const rle_str = line.substr(quote1 + 1, quote2 - quote1 - 1);
        Mask2D mask = decode_mask_rle(rle_str, rle_delimiter);

        data_map[TimeFrameIndex(frame_num)].push_back(std::move(mask));
        ++loaded_masks;
    }

    file.close();
    std::cout << "Loaded " << loaded_masks << " masks from " << opts.filepath << std::endl;
    return data_map;
}

bool save(MaskData const * mask_data, CSVMaskRLESaverOptions const & opts) {
    assert(mask_data && "save: mask_data must not be null");

    auto const target_path = std::filesystem::path(opts.parent_dir) / opts.filename;

    bool const ok = atomicWriteFile(target_path, [&](std::ostream & out) {
        if (opts.save_header && !opts.header.empty()) {
            out << opts.header << "\n";
        }

        for (auto const & [time, entity_id, mask]: mask_data->flattened_data()) {
            std::string const rle_str = encode_mask_rle(mask, opts.rle_delimiter);
            out << time.getValue() << opts.delimiter << "\"" << rle_str << "\"\n";
        }
        return out.good();
    });

    if (ok) {
        std::cout << "Successfully saved mask data (RLE) to " << target_path << std::endl;
    }
    return ok;
}
