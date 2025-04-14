#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "LockState/LockState.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "lines.hpp"

#include <map>
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
    void clearLineAtTime(int time, int line_id);
    void clearLinesAtTime(int time);
    void addLineAtTime(int time, std::vector<float> const & x, std::vector<float> const & y);
    void addLineAtTime(int time, std::vector<Point2D<float>> const & line);

    void addPointToLine(int time, int line_id, Point2D<float> point);

    void addPointToLineInterpolate(int time, int line_id, Point2D<float> point);

    [[nodiscard]] std::vector<int> getTimesWithLines() const;

    [[nodiscard]] std::vector<Line2D> const & getLinesAtTime(int time) const;

    [[nodiscard]] std::map<int, std::vector<Line2D>> const & getData() const { return _data; };

    void lockTime(int time) { _lock_state.lock(time); }
    void unlockTime(int time) { _lock_state.unlock(time); }
    [[nodiscard]] bool isTimeLocked(int time) const { return _lock_state.isLocked(time); }

    void lockUntil(int time) {
        _lock_state.clear();
        for (int i = 0; i <= time; i++) {
            _lock_state.lock(i);
        }
    }

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

protected:
private:
    std::map<int, std::vector<Line2D>> _data;
    std::vector<Line2D> _empty{};
    LockState _lock_state;
    ImageSize _image_size;
};


#endif// LINE_DATA_HPP
