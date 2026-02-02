#ifndef POINT_CSV_LOADING_SCENARIOS_HPP
#define POINT_CSV_LOADING_SCENARIOS_HPP

#include "fixtures/builders/PointDataBuilder.hpp"
#include "Points/Point_Data.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <map>

namespace point_csv_scenarios {

/**
 * @brief Helper to write PointData to simple CSV with frame, x, y columns
 * 
 * Writes three-column CSV with optional header.
 * 
 * @param points The point data as a map of time to point
 * @param filepath Output file path
 * @param delimiter Column delimiter (default ",")
 * @param write_header Whether to write header row
 * @param header_text Header text if write_header is true
 * @return true if write succeeded
 */
inline bool writeCSVSimple(std::map<TimeFrameIndex, Point2D<float>> const& points,
                           std::string const& filepath,
                           std::string const& delimiter = ",",
                           bool write_header = true,
                           std::string const& header_text = "frame,x,y") {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header if requested
    if (write_header) {
        file << header_text << "\n";
    }
    
    // Write data rows
    for (auto const& [time, point] : points) {
        file << time.getValue() << delimiter << point.x << delimiter << point.y << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write PointData without header
 * 
 * @param points The point data as a map of time to point
 * @param filepath Output file path
 * @param delimiter Column delimiter
 * @return true if write succeeded
 */
inline bool writeCSVNoHeader(std::map<TimeFrameIndex, Point2D<float>> const& points,
                             std::string const& filepath,
                             std::string const& delimiter = ",") {
    return writeCSVSimple(points, filepath, delimiter, false, "");
}

/**
 * @brief Helper to write PointData with custom delimiter
 * 
 * @param points The point data as a map of time to point
 * @param filepath Output file path
 * @param delimiter Column delimiter
 * @return true if write succeeded
 */
inline bool writeCSVWithDelimiter(std::map<TimeFrameIndex, Point2D<float>> const& points,
                                  std::string const& filepath,
                                  std::string const& delimiter) {
    // For non-comma delimiters, adjust the header
    std::string header = "frame" + delimiter + "x" + delimiter + "y";
    return writeCSVSimple(points, filepath, delimiter, true, header);
}

/**
 * @brief Helper to write PointData with space delimiter (no header, typical format)
 * 
 * @param points The point data as a map of time to point
 * @param filepath Output file path
 * @return true if write succeeded
 */
inline bool writeCSVSpaceDelimited(std::map<TimeFrameIndex, Point2D<float>> const& points,
                                   std::string const& filepath) {
    return writeCSVSimple(points, filepath, " ", false, "");
}

/**
 * @brief Helper to write DLC format CSV file
 * 
 * Writes DeepLabCut format with:
 * - Row 1: scorer name
 * - Row 2: bodypart names (repeated 3x for x, y, likelihood per bodypart)
 * - Row 3: coord types (x, y, likelihood repeated)
 * - Data rows: frame index, then x, y, likelihood for each bodypart
 * 
 * @param bodypart_data Map of bodypart name to (time -> point) data
 * @param filepath Output file path
 * @param scorer_name Name of the scorer (default "DLC_resnet50_testJan1shuffle1_100000")
 * @return true if write succeeded
 */
inline bool writeDLCFormat(
        std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> const& bodypart_data,
        std::string const& filepath,
        std::string const& scorer_name = "DLC_resnet50_testJan1shuffle1_100000") {
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Collect bodypart names in order
    std::vector<std::string> bodyparts;
    for (auto const& [name, _] : bodypart_data) {
        bodyparts.push_back(name);
    }
    
    // Row 1: scorer (empty first column, then scorer name repeated for each coord)
    file << "scorer";
    for (size_t i = 0; i < bodyparts.size() * 3; ++i) {
        file << "," << scorer_name;
    }
    file << "\n";
    
    // Row 2: bodyparts (empty first column, then each bodypart repeated 3x)
    file << "bodyparts";
    for (auto const& bp : bodyparts) {
        file << "," << bp << "," << bp << "," << bp;
    }
    file << "\n";
    
    // Row 3: coords (empty first column, then x,y,likelihood repeated)
    file << "coords";
    for (size_t i = 0; i < bodyparts.size(); ++i) {
        file << ",x,y,likelihood";
    }
    file << "\n";
    
    // Collect all unique frame numbers
    std::set<TimeFrameIndex> all_frames;
    for (auto const& [_, points] : bodypart_data) {
        for (auto const& [time, __] : points) {
            all_frames.insert(time);
        }
    }
    
    // Data rows
    for (auto const& frame : all_frames) {
        file << frame.getValue();
        
        for (auto const& bp : bodyparts) {
            auto it = bodypart_data.find(bp);
            if (it != bodypart_data.end()) {
                auto point_it = it->second.find(frame);
                if (point_it != it->second.end()) {
                    // Point exists for this frame and bodypart
                    file << "," << point_it->second.x 
                         << "," << point_it->second.y 
                         << "," << 0.99;  // High likelihood by default
                } else {
                    // No point for this frame - write NaN or zeros
                    file << ",0,0,0";
                }
            } else {
                file << ",0,0,0";
            }
        }
        file << "\n";
    }
    
    return file.good();
}

/**
 * @brief Helper to write DLC format with varying likelihoods
 * 
 * @param bodypart_data Map of bodypart name to (time -> (point, likelihood)) data
 * @param filepath Output file path
 * @param scorer_name Name of the scorer
 * @return true if write succeeded
 */
inline bool writeDLCFormatWithLikelihood(
        std::map<std::string, std::map<TimeFrameIndex, std::pair<Point2D<float>, float>>> const& bodypart_data,
        std::string const& filepath,
        std::string const& scorer_name = "DLC_resnet50_testJan1shuffle1_100000") {
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Collect bodypart names in order
    std::vector<std::string> bodyparts;
    for (auto const& [name, _] : bodypart_data) {
        bodyparts.push_back(name);
    }
    
    // Row 1: scorer
    file << "scorer";
    for (size_t i = 0; i < bodyparts.size() * 3; ++i) {
        file << "," << scorer_name;
    }
    file << "\n";
    
    // Row 2: bodyparts
    file << "bodyparts";
    for (auto const& bp : bodyparts) {
        file << "," << bp << "," << bp << "," << bp;
    }
    file << "\n";
    
    // Row 3: coords
    file << "coords";
    for (size_t i = 0; i < bodyparts.size(); ++i) {
        file << ",x,y,likelihood";
    }
    file << "\n";
    
    // Collect all unique frame numbers
    std::set<TimeFrameIndex> all_frames;
    for (auto const& [_, points] : bodypart_data) {
        for (auto const& [time, __] : points) {
            all_frames.insert(time);
        }
    }
    
    // Data rows
    for (auto const& frame : all_frames) {
        file << frame.getValue();
        
        for (auto const& bp : bodyparts) {
            auto it = bodypart_data.find(bp);
            if (it != bodypart_data.end()) {
                auto point_it = it->second.find(frame);
                if (point_it != it->second.end()) {
                    auto const& [point, likelihood] = point_it->second;
                    file << "," << point.x 
                         << "," << point.y 
                         << "," << likelihood;
                } else {
                    file << ",0,0,0";
                }
            } else {
                file << ",0,0,0";
            }
        }
        file << "\n";
    }
    
    return file.good();
}

//=============================================================================
// Pre-configured test point data for CSV loading tests
//=============================================================================

/**
 * @brief Simple point data with 5 frames
 * 
 * Creates points at frames 0, 10, 20, 30, 40 with simple x,y coordinates
 */
inline std::map<TimeFrameIndex, Point2D<float>> simple_points() {
    return {
        {TimeFrameIndex(0), Point2D<float>{10.5f, 20.5f}},
        {TimeFrameIndex(10), Point2D<float>{15.0f, 25.0f}},
        {TimeFrameIndex(20), Point2D<float>{20.5f, 30.5f}},
        {TimeFrameIndex(30), Point2D<float>{25.0f, 35.0f}},
        {TimeFrameIndex(40), Point2D<float>{30.5f, 40.5f}}
    };
}

/**
 * @brief Single point for minimal test case
 */
inline std::map<TimeFrameIndex, Point2D<float>> single_point() {
    return {
        {TimeFrameIndex(100), Point2D<float>{50.0f, 60.0f}}
    };
}

/**
 * @brief Dense sequential points (every frame)
 */
inline std::map<TimeFrameIndex, Point2D<float>> dense_points() {
    std::map<TimeFrameIndex, Point2D<float>> result;
    for (int i = 0; i < 10; ++i) {
        result[TimeFrameIndex(i)] = Point2D<float>{
            static_cast<float>(i * 5),
            static_cast<float>(i * 10)
        };
    }
    return result;
}

/**
 * @brief Sparse points with large gaps
 */
inline std::map<TimeFrameIndex, Point2D<float>> sparse_points() {
    return {
        {TimeFrameIndex(0), Point2D<float>{1.0f, 2.0f}},
        {TimeFrameIndex(1000), Point2D<float>{100.0f, 200.0f}},
        {TimeFrameIndex(5000), Point2D<float>{500.0f, 1000.0f}}
    };
}

/**
 * @brief Points with negative coordinates
 */
inline std::map<TimeFrameIndex, Point2D<float>> negative_coord_points() {
    return {
        {TimeFrameIndex(0), Point2D<float>{-10.5f, -20.5f}},
        {TimeFrameIndex(10), Point2D<float>{-5.0f, 15.0f}},
        {TimeFrameIndex(20), Point2D<float>{0.0f, 0.0f}},
        {TimeFrameIndex(30), Point2D<float>{5.0f, -15.0f}}
    };
}

/**
 * @brief Points with decimal precision
 */
inline std::map<TimeFrameIndex, Point2D<float>> decimal_precision_points() {
    return {
        {TimeFrameIndex(0), Point2D<float>{100.123f, 200.456f}},
        {TimeFrameIndex(1), Point2D<float>{101.789f, 201.012f}},
        {TimeFrameIndex(2), Point2D<float>{102.345f, 202.678f}}
    };
}

/**
 * @brief Multi-bodypart DLC data with 2 bodyparts
 */
inline std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> two_bodypart_dlc() {
    return {
        {"nose", {
            {TimeFrameIndex(0), Point2D<float>{100.0f, 150.0f}},
            {TimeFrameIndex(1), Point2D<float>{101.0f, 151.0f}},
            {TimeFrameIndex(2), Point2D<float>{102.0f, 152.0f}},
            {TimeFrameIndex(3), Point2D<float>{103.0f, 153.0f}},
            {TimeFrameIndex(4), Point2D<float>{104.0f, 154.0f}}
        }},
        {"tail", {
            {TimeFrameIndex(0), Point2D<float>{200.0f, 250.0f}},
            {TimeFrameIndex(1), Point2D<float>{201.0f, 251.0f}},
            {TimeFrameIndex(2), Point2D<float>{202.0f, 252.0f}},
            {TimeFrameIndex(3), Point2D<float>{203.0f, 253.0f}},
            {TimeFrameIndex(4), Point2D<float>{204.0f, 254.0f}}
        }}
    };
}

/**
 * @brief Multi-bodypart DLC data with 3 bodyparts
 */
inline std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> three_bodypart_dlc() {
    return {
        {"head", {
            {TimeFrameIndex(0), Point2D<float>{50.0f, 60.0f}},
            {TimeFrameIndex(10), Point2D<float>{55.0f, 65.0f}},
            {TimeFrameIndex(20), Point2D<float>{60.0f, 70.0f}}
        }},
        {"body", {
            {TimeFrameIndex(0), Point2D<float>{100.0f, 110.0f}},
            {TimeFrameIndex(10), Point2D<float>{105.0f, 115.0f}},
            {TimeFrameIndex(20), Point2D<float>{110.0f, 120.0f}}
        }},
        {"tail", {
            {TimeFrameIndex(0), Point2D<float>{150.0f, 160.0f}},
            {TimeFrameIndex(10), Point2D<float>{155.0f, 165.0f}},
            {TimeFrameIndex(20), Point2D<float>{160.0f, 170.0f}}
        }}
    };
}

/**
 * @brief DLC data with varying likelihoods for testing threshold filtering
 */
inline std::map<std::string, std::map<TimeFrameIndex, std::pair<Point2D<float>, float>>> 
dlc_with_likelihoods() {
    return {
        {"nose", {
            {TimeFrameIndex(0), {{100.0f, 150.0f}, 0.99f}},  // High confidence
            {TimeFrameIndex(1), {{101.0f, 151.0f}, 0.85f}},  // Medium confidence
            {TimeFrameIndex(2), {{102.0f, 152.0f}, 0.50f}},  // Low confidence
            {TimeFrameIndex(3), {{103.0f, 153.0f}, 0.10f}},  // Very low confidence
            {TimeFrameIndex(4), {{104.0f, 154.0f}, 0.95f}}   // High confidence
        }},
        {"ear", {
            {TimeFrameIndex(0), {{200.0f, 250.0f}, 0.92f}},
            {TimeFrameIndex(1), {{201.0f, 251.0f}, 0.40f}},
            {TimeFrameIndex(2), {{202.0f, 252.0f}, 0.88f}},
            {TimeFrameIndex(3), {{203.0f, 253.0f}, 0.70f}},
            {TimeFrameIndex(4), {{204.0f, 254.0f}, 0.15f}}
        }}
    };
}

/**
 * @brief Single bodypart DLC for minimal test case
 */
inline std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> single_bodypart_dlc() {
    return {
        {"point", {
            {TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}},
            {TimeFrameIndex(1), Point2D<float>{11.0f, 21.0f}},
            {TimeFrameIndex(2), Point2D<float>{12.0f, 22.0f}}
        }}
    };
}

} // namespace point_csv_scenarios

#endif // POINT_CSV_LOADING_SCENARIOS_HPP
