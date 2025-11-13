
#ifndef WHISKERTOOLBOX_XAXIS_HPP
#define WHISKERTOOLBOX_XAXIS_HPP

#include <cstdint>
#include <iostream>

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
    explicit XAxis(int64_t start = 0, int64_t end = 100, int64_t min = 0, int64_t max = 1000)
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
        int64_t const halfRange = zoom / 2;
        _start = center - halfRange;
        _end = center + halfRange;
        clampVisibleRange();
        if (_start >= _end) _end = _start + 1;// Ensure _start is less than _end
    }

    // Returns the actual range width that was set (may be different due to clamping)
    int64_t setCenterAndZoomWithFeedback(int64_t center, int64_t range_width) {
        int64_t const halfRange = range_width / 2;
        int64_t const original_start = center - halfRange;
        int64_t const original_end = center + halfRange + (range_width % 2);  // Add 1 if range_width is odd
        
        _start = original_start;
        _end = original_end;
        
        std::cout << "XAxis: Requested center=" << center << ", range_width=" << range_width 
                  << ", range=[" << _start << ", " << _end << "]" << std::endl;
        std::cout << "XAxis: Bounds=[" << _min << ", " << _max << "]" << std::endl;
        
        clampVisibleRange();
        
        if (_start >= _end) _end = _start + 1;// Ensure _start is less than _end
        
        int64_t const actual_range_width = _end - _start;
        std::cout << "XAxis: Final range=[" << _start << ", " << _end << "], actual_range_width=" << actual_range_width << std::endl;
        
        return actual_range_width; // Return actual range width achieved
    }

    [[nodiscard]] int64_t getStart() const { return _start; }
    [[nodiscard]] int64_t getEnd() const { return _end; }
    [[nodiscard]] int64_t getMin() const { return _min; }
    [[nodiscard]] int64_t getMax() const { return _max; }
    void setMax(int64_t max) {
        std::cout << "XAxis::setMax called: old_max=" << _max << ", new_max=" << max << std::endl;
        _max = max;
    }

private:
    void clampVisibleRange() {
        // Calculate the desired range width before clamping
        int64_t const range_width = _end - _start;
        
        if (_start < _min) {
            _start = _min;
            _end = _start + range_width;
        }
        if (_end > _max) {
            _end = _max;
            _start = _end - range_width;
        }
        if (_start < _min) {
            _start = _min;     // Re-check after adjusting _start
        }
        if (_start >= _end) _end = _start + 1;// Ensure _start is less than _end
    }

    int64_t _start;
    int64_t _end;
    int64_t _min;
    int64_t _max;
};


#endif//WHISKERTOOLBOX_XAXIS_HPP
