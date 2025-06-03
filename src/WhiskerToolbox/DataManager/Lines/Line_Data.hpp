#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "lines.hpp"

#include <cstddef>
#include <map>
#include <ranges>
#include <vector>

/*
 * @brief LineData
 *
 * LineData is used for storing 2D lines
 * Line data implies that the elements in the line have an order
 * Compare to MaskData where the elements in the mask have no order
 */
class LineData : public ObserverData {
public:
    LineData() = default;
    explicit LineData(std::map<int, std::vector<Line2D>> const & data)
        : _data(data){};
    void clearLineAtTime(int time, int line_id, bool notify = true);
    void clearLinesAtTime(int time, bool notify = true);
    void addLineAtTime(int time, std::vector<float> const & x, std::vector<float> const & y, bool notify = true);
    void addLineAtTime(int time, std::vector<Point2D<float>> const & line, bool notify = true);

    void addPointToLine(int time, int line_id, Point2D<float> point, bool notify = true);

    void addPointToLineInterpolate(int time, int line_id, Point2D<float> point, bool notify = true);

    [[nodiscard]] std::vector<int> getTimesWithData() const;

    [[nodiscard]] std::vector<Line2D> const & getLinesAtTime(int time) const;

    /**
     * @brief Copy lines from this LineData to another LineData for a time range
     * 
     * Copies all lines within the specified time range [start_time, end_time] (inclusive)
     * to the target LineData. If lines already exist at target times, the copied lines
     * are added to the existing lines.
     * 
     * @param target The target LineData to copy lines to
     * @param start_time The starting time (inclusive)
     * @param end_time The ending time (inclusive)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of lines actually copied
     */
    std::size_t copyTo(LineData& target, int start_time, int end_time, bool notify = true) const;

    /**
     * @brief Copy lines from this LineData to another LineData for specific times
     * 
     * Copies all lines at the specified times to the target LineData.
     * If lines already exist at target times, the copied lines are added to the existing lines.
     * 
     * @param target The target LineData to copy lines to
     * @param times Vector of specific times to copy (does not need to be sorted)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of lines actually copied
     */
    std::size_t copyTo(LineData& target, std::vector<int> const& times, bool notify = true) const;

    /**
     * @brief Move lines from this LineData to another LineData for a time range
     * 
     * Moves all lines within the specified time range [start_time, end_time] (inclusive)
     * to the target LineData. Lines are copied to target then removed from source.
     * If lines already exist at target times, the moved lines are added to the existing lines.
     * 
     * @param target The target LineData to move lines to
     * @param start_time The starting time (inclusive)
     * @param end_time The ending time (inclusive)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of lines actually moved
     */
    std::size_t moveTo(LineData& target, int start_time, int end_time, bool notify = true);

    /**
     * @brief Move lines from this LineData to another LineData for specific times
     * 
     * Moves all lines at the specified times to the target LineData.
     * Lines are copied to target then removed from source.
     * If lines already exist at target times, the moved lines are added to the existing lines.
     * 
     * @param target The target LineData to move lines to
     * @param times Vector of specific times to move (does not need to be sorted)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of lines actually moved
     */
    std::size_t moveTo(LineData& target, std::vector<int> const& times, bool notify = true);

    /**
     * @brief Change the size of the canvas the line belongs to
     *
     * This will scale all lines in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size
     */
    void changeImageSize(ImageSize const & image_size);

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }




    /**
    * @brief Get all lines with their associated times as a range
    *
    * @return A view of time-lines pairs for all times
    */
    [[nodiscard]] auto GetAllLinesAsRange() const {
        struct TimeLinesPair {
            int time;
            std::vector<Line2D> const & lines;
        };

        return _data | std::views::transform([](auto const & pair) {
                   return TimeLinesPair{pair.first, pair.second};
               });
    }

protected:
private:
    std::map<int, std::vector<Line2D>> _data;
    std::vector<Line2D> _empty{};
    ImageSize _image_size;
};


#endif// LINE_DATA_HPP
