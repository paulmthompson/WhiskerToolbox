#include "Serialization.hpp"
#include "line_data.capnp.h"

// Include the actual LineData implementation
#include "Lines/Line_Data.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <vector>
#include <iostream>

namespace IO::CapnProto {

kj::Array<capnp::word> serializeLineData(LineData const* lineData) {
    capnp::MallocMessageBuilder message;
    LineDataProto::Builder lineDataProto = message.initRoot<LineDataProto>();

    auto times = lineData->getTimesWithData();

    auto timeLinesList = lineDataProto.initTimeLines(times.size());
 
    size_t i = 0;
    for (auto [time, entries] : lineData->getAllEntries()) {
        auto timeLine = timeLinesList[i];
        timeLine.setTime(time.getValue());

        auto linesList = timeLine.initLines(entries.size());

        for (size_t j = 0; j < entries.size(); j++) {
            auto lineBuilder = linesList[j];
            auto pointsList = lineBuilder.initPoints(entries[j].data.size());

            for (size_t k = 0; k < entries[j].data.size(); k++) {
                auto pointBuilder = pointsList[k];
                pointBuilder.setX(entries[j].data[k].x);
                pointBuilder.setY(entries[j].data[k].y);
            }
        }

        i++;
    }

    ImageSize const imgSize = lineData->getImageSize();
    if (imgSize.width > 0 && imgSize.height > 0) {
        lineDataProto.setImageWidth(static_cast<uint32_t>(imgSize.width));
        lineDataProto.setImageHeight(static_cast<uint32_t>(imgSize.height));
    }

    kj::Array<capnp::word> words = capnp::messageToFlatArray(message);
    return words;
}

std::shared_ptr<LineData> deserializeLineData(
    kj::ArrayPtr<capnp::word const> messageData,
    capnp::ReaderOptions const& options)
{
    capnp::FlatArrayMessageReader message(messageData, options);
    LineDataProto::Reader lineDataProto = message.getRoot<LineDataProto>();

    std::map<TimeFrameIndex, std::vector<Line2D>> dataMap;
    for (auto timeLine: lineDataProto.getTimeLines()) {
        TimeFrameIndex const time = TimeFrameIndex(timeLine.getTime());
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

    auto lineData = std::make_shared<LineData>(dataMap);

    uint32_t const width = lineDataProto.getImageWidth();
    uint32_t const height = lineDataProto.getImageHeight();
    if (width > 0 && height > 0) {
        lineData->setImageSize(ImageSize{static_cast<int>(width), static_cast<int>(height)});
    }

    return lineData;
}

} // namespace IO::CapnProto
