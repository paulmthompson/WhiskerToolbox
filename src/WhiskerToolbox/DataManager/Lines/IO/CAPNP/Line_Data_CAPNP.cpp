
#include "Line_Data_CAPNP.hpp"

#include "line_data.capnp.h"

#include "Lines/Line_Data.hpp"

#include <vector>

// Convert LineData to Cap'n Proto message
kj::Array<capnp::word> serializeLineData(LineData const * lineData) {
    capnp::MallocMessageBuilder message;
    LineDataProto::Builder lineDataProto = message.initRoot<LineDataProto>();

    std::vector<int> times = lineData->getTimesWithLines();

    std::cout << "There are " << times.size() << " time frames" << std::endl;

    auto timeLinesList = lineDataProto.initTimeLines(times.size());

    // Fill in the data
    for (size_t i = 0; i < times.size(); i++) {
        int const time = times[i];
        auto timeLine = timeLinesList[i];

        // Set the time
        timeLine.setTime(time);

        // Get the lines for this time
        std::vector<Line2D> const & lines = lineData->getLinesAtTime(time);

        // Initialize the lines list with the correct size
        auto linesList = timeLine.initLines(lines.size());

        // Fill in each line
        for (size_t j = 0; j < lines.size(); j++) {
            Line2D const & line = lines[j];
            auto lineBuilder = linesList[j];

            // Initialize points list
            auto pointsList = lineBuilder.initPoints(line.size());

            // Fill in each point
            for (size_t k = 0; k < line.size(); k++) {
                auto pointBuilder = pointsList[k];
                pointBuilder.setX(line[k].x);
                pointBuilder.setY(line[k].y);
            }
        }
    }

    // Set image size if available
    ImageSize const imgSize = lineData->getImageSize();
    if (imgSize.width > 0 && imgSize.height > 0) {
        lineDataProto.setImageWidth(static_cast<uint32_t>(imgSize.width));
        lineDataProto.setImageHeight(static_cast<uint32_t>(imgSize.height));
    }

    kj::Array<capnp::word> words = capnp::messageToFlatArray(message);
    return words;
}

// Convert Cap'n Proto message to LineData
std::shared_ptr<LineData> deserializeLineData(kj::ArrayPtr<capnp::word const> messageData) {
    capnp::FlatArrayMessageReader message(messageData);
    LineDataProto::Reader lineDataProto = message.getRoot<LineDataProto>();

    // Create a new LineData object
    auto lineData = std::make_shared<LineData>();

    // Read image size if available
    uint32_t const width = lineDataProto.getImageWidth();
    uint32_t const height = lineDataProto.getImageHeight();
    if (width > 0 && height > 0) {
        lineData->setImageSize(ImageSize{static_cast<int>(width), static_cast<int>(height)});
    }

    // Process all time lines
    std::map<int, std::vector<Line2D>> dataMap;
    for (auto timeLine: lineDataProto.getTimeLines()) {
        int const time = timeLine.getTime();
        std::vector<Line2D> lines;

        for (auto line: timeLine.getLines()) {
            Line2D currentLine;

            for (auto point: line.getPoints()) {
                currentLine.push_back(Point2D<float>{point.getX(), point.getY()});
            }

            lines.push_back(currentLine);
        }

        dataMap[time] = lines;
    }

    // Create a new LineData with the deserialized data
    return std::make_shared<LineData>(dataMap);
}
