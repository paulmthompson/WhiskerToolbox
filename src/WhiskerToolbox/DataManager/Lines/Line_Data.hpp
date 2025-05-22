#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "lines.hpp"

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
