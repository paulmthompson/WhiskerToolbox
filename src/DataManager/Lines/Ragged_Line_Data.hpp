#ifndef RAGGED_LINE_DATA_HPP
#define RAGGED_LINE_DATA_HPP

#include "Line_Data.hpp"

/**
 * @brief RaggedLineData - Variable-sized line storage
 * 
 * RaggedLineData stores a variable number of lines at each time point.
 * This is the current default behavior and supports ragged/jagged arrays
 * where each time frame can have a different number of lines.
 * 
 * This class provides an explicit type for ragged line data that can be
 * used when the specific storage pattern needs to be indicated.
 * 
 * Functionally, this is identical to the base LineData class.
 */
class RaggedLineData : public LineData {
public:
    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * Creates an empty RaggedLineData with no data
     */
    RaggedLineData() = default;

    /**
     * @brief Constructor with data
     * 
     * Creates a RaggedLineData with the given data
     * 
     * @param data The data to initialize with
     */
    explicit RaggedLineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data)
        : LineData(data) {}

    /**
     * @brief Get the line data type
     * @return LineDataType enum value
     */
    LineDataType getLineDataType() const override { return LineDataType::Ragged; }
};

#endif// RAGGED_LINE_DATA_HPP
