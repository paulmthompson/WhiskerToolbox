#include "CapnProtoLoader.hpp"
#include "../LoaderRegistry.hpp"

// Only include CapnProto-specific headers, no data type dependencies
#include "line_data.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/std/iostream.h>

#include <fstream>
#include <iostream>

CapnProtoLoader::CapnProtoLoader() {
    // Register supported data types
    _supported_types.insert(IODataType::Line);
    // Future: _supported_types.insert(IODataType::Points);
    // Future: _supported_types.insert(IODataType::Mask);
}

std::string CapnProtoLoader::getFormatId() const {
    return "capnp";
}

bool CapnProtoLoader::supportsDataType(IODataType data_type) const {
    return _supported_types.find(data_type) != _supported_types.end();
}

LoadResult CapnProtoLoader::loadData(
    std::string const& file_path,
    IODataType data_type,
    nlohmann::json const& config,
    DataFactory* factory
) const {
    switch (data_type) {
        case IODataType::Line:
            return loadLineData(file_path, config, factory);
        
        default:
            return {"CapnProto loader does not support data type: " + 
                            std::to_string(static_cast<int>(data_type))};
    }
}

LoadResult CapnProtoLoader::loadLineData(std::string const& file_path, nlohmann::json const& config, DataFactory* factory) const {
    if (!factory) {
        return {"DataFactory is null"};
    }
    
    try {
        // Open the file
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return {"Failed to open CapnProto file: " + file_path};
        }
        
        // Read the entire file into memory
        file.seekg(0, std::ios::end);
        size_t const file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<char> buffer(file_size);
        file.read(buffer.data(), static_cast<std::streamsize>(file_size));
        file.close();
        
        // Convert to capnp words
        kj::ArrayPtr<capnp::word const> const words = 
            kj::arrayPtr(reinterpret_cast<capnp::word const*>(buffer.data()), 
                        file_size / sizeof(capnp::word));
        
        // Extract raw data from CapnProto
        capnp::ReaderOptions options;
        constexpr size_t TRAVERSAL_LIMIT_BUFFER = 1000;
        options.traversalLimitInWords = file_size / sizeof(capnp::word) + TRAVERSAL_LIMIT_BUFFER;
        
        LineDataRaw raw_data = extractLineDataRaw(words, options);
        
        // Use factory to create the proper data object from raw data
        auto data_variant = factory->createLineDataFromRaw(raw_data);
        
        return {std::move(data_variant)};
        
    } catch (std::exception const& e) {
        return {"CapnProto loading error: " + std::string(e.what())};
    }
}

LineDataRaw CapnProtoLoader::extractLineDataRaw(
    kj::ArrayPtr<capnp::word const> messageData,
    capnp::ReaderOptions const& options) const {
    
    capnp::FlatArrayMessageReader message(messageData, options);
    LineDataProto::Reader const lineDataProto = message.getRoot<LineDataProto>();

    LineDataRaw raw_data;
    raw_data.image_width = lineDataProto.getImageWidth();
    raw_data.image_height = lineDataProto.getImageHeight();

    for (auto timeLine: lineDataProto.getTimeLines()) {
        int32_t const time = timeLine.getTime();
        std::vector<Line2D> lines_raw;

        for (auto line: timeLine.getLines()) {
            Line2D line_raw;

            for (auto point: line.getPoints()) {
                line_raw.push_back(Point2D(point.getX(), point.getY()));
            }

            lines_raw.push_back(line_raw);
        }

        raw_data.time_lines[time] = lines_raw;
    }

    return raw_data;
}

// Note: CapnProto registration is now handled by the LoaderRegistration system
// The CapnProtoFormatLoader wraps this class for the new registry system
