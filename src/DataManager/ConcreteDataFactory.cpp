#include "ConcreteDataFactory.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"


LoadedDataVariant ConcreteDataFactory::createLineData() {
    return std::make_shared<LineData>();
}

LoadedDataVariant ConcreteDataFactory::createLineData(std::map<TimeFrameIndex, std::vector<Line2D>> const& data) {
    return std::make_shared<LineData>(data);
}

LoadedDataVariant ConcreteDataFactory::createLineDataFromRaw(LineDataRaw const& raw_data) {
    auto converted_data = convertRawLineData(raw_data);
        auto line_data = std::make_shared<LineData>(converted_data);
        
        // Set image size if available
        if (raw_data.image_width > 0 && raw_data.image_height > 0) {
            line_data->setImageSize(ImageSize{
                static_cast<int>(raw_data.image_width), 
                static_cast<int>(raw_data.image_height)
            });
        }
        
        return line_data;
}

void ConcreteDataFactory::setLineDataImageSize(LoadedDataVariant& data, int width, int height) {
    if (std::holds_alternative<std::shared_ptr<LineData>>(data)) {
        auto line_data = std::get<std::shared_ptr<LineData>>(data);
        if (line_data) {
            line_data->setImageSize(ImageSize{width, height});
        }
    }
    
};

// MaskData factory methods
LoadedDataVariant ConcreteDataFactory::createMaskData() {
    return std::make_shared<MaskData>();
}

LoadedDataVariant ConcreteDataFactory::createMaskDataFromRaw(MaskDataRaw const& raw_data) {
    auto converted_data = convertRawMaskData(raw_data);
    auto mask_data = std::make_shared<MaskData>();
    
    // Add each mask to the MaskData
    for (auto const& [time, masks] : converted_data) {
        for (auto const& mask : masks) {
            mask_data->addAtTime(time, mask, NotifyObservers::No);
        }
    }
    
    // Set image size if available
    if (raw_data.image_width > 0 && raw_data.image_height > 0) {
        mask_data->setImageSize(ImageSize{
            static_cast<int>(raw_data.image_width), 
            static_cast<int>(raw_data.image_height)
        });
    }
    
    return mask_data;
}

void ConcreteDataFactory::setMaskDataImageSize(LoadedDataVariant& data, int width, int height) {
    if (std::holds_alternative<std::shared_ptr<MaskData>>(data)) {
        auto mask_data = std::get<std::shared_ptr<MaskData>>(data);
        if (mask_data) {
            mask_data->setImageSize(ImageSize{width, height});
        }
    }
}

/**
 * @brief Helper function to convert raw line data to proper types
 */
std::map<TimeFrameIndex, std::vector<Line2D>> convertRawLineData(LineDataRaw const& raw_data) {
    std::map<TimeFrameIndex, std::vector<Line2D>> result;
    
    for (auto const& [time_int, lines_raw] : raw_data.time_lines) {
        TimeFrameIndex time(time_int);
        std::vector<Line2D> lines;
        
        for (auto const& line_raw : lines_raw) {
            Line2D line;
            for (auto const& [x, y] : line_raw) {
                line.push_back(Point2D<float>{x, y});
            }
            lines.push_back(line);
        }
        
        result[time] = lines;
    }
    
    return result;
}

/**
 * @brief Helper function to convert raw mask data to proper types
 */
std::map<TimeFrameIndex, std::vector<std::vector<Point2D<uint32_t>>>> convertRawMaskData(MaskDataRaw const& raw_data) {
    std::map<TimeFrameIndex, std::vector<std::vector<Point2D<uint32_t>>>> result;
    
    for (auto const& [time_int, masks_raw] : raw_data.time_masks) {
        TimeFrameIndex time(time_int);
        std::vector<std::vector<Point2D<uint32_t>>> masks;
        
        for (auto const& mask_raw : masks_raw) {
            std::vector<Point2D<uint32_t>> mask;
            for (auto const& [x, y] : mask_raw) {
                mask.push_back(Point2D<uint32_t>{x, y});
            }
            masks.push_back(mask);
        }
        
        result[time] = masks;
    }
    
    return result;
}
