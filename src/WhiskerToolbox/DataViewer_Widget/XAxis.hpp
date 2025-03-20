
#ifndef WHISKERTOOLBOX_XAXIS_HPP
#define WHISKERTOOLBOX_XAXIS_HPP

#include <cstdint>

/**
 * @brief The XAxis class represents the x-axis of the DataViewer
 *
 *
 *
 * start = minimum visible value
 * end = maximum visible value
 *
 * min = minimum value possible
 * max = maximum value possible
 */
class XAxis {
public:
    XAxis(int64_t start = 0, int64_t end = 100, int64_t min = 0, int64_t max = 1000)
        : _start(start),
          _end(end),
          _min(min),
          _max(max) {
        if (_start < _min) _start = _min;
        if (_end > _max) _end = _max;
        if (_start >= _end) _end = _start + 1;// Ensure _start is less than _end
    }

    void setVisibleRange(int64_t start, int64_t end) {
        if (start < _min) start = _min;
        if (end > _max) end = _max;
        _start = start;
        _end = end;
        if (start >= end) end = start + 1;// Ensure _start is less than _end
        _start = start;
        _end = end;
    }

    void setCenterAndZoom(int64_t center, int64_t zoom) {
        int64_t halfRange = zoom / 2;
        _start = center - halfRange;
        _end = center + halfRange;
        clampVisibleRange();
        if (_start >= _end) _end = _start + 1;// Ensure _start is less than _end
    }

    int64_t getStart() const { return _start; }
    int64_t getEnd() const { return _end; }
    int64_t getMin() const { return _min; }
    int64_t getMax() const { return _max; }
    void setMax(int64_t max) { _max = max; }

private:
    void clampVisibleRange() {
        if (_start < _min) {
            _start = _min;
            _end = _min + (_end - _start);
        }
        if (_end > _max) {
            _end = _max;
            _start = _max - (_end - _start);
        }
        if (_start < _min) _start = _min;     // Re-check after adjusting _end
        if (_start >= _end) _end = _start + 1;// Ensure _start is less than _end
    }

    int64_t _start;
    int64_t _end;
    int64_t _min;
    int64_t _max;
};


#endif//WHISKERTOOLBOX_XAXIS_HPP
