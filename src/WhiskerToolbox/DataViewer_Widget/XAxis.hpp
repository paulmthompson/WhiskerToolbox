
#ifndef WHISKERTOOLBOX_XAXIS_HPP
#define WHISKERTOOLBOX_XAXIS_HPP

class XAxis {
public:
    XAxis(int start = 0, int end = 100, int min = 0, int max = 1000)
            : _start(start), _end(end), _min(min), _max(max) {
        if (_start < _min) _start = _min;
        if (_end > _max) _end = _max;
        if (_start >= _end) _end = _start + 1; // Ensure _start is less than _end
    }

    void setRange(int start, int end) {
        if (start < _min) start = _min;
        if (end > _max) end = _max;
        _start = start;
        _end = end;
        if (start >= end) end = start + 1; // Ensure _start is less than _end
        _start = start;
        _end = end;
    }

    void setCenterAndZoom(int center, int zoom) {
        int halfRange = zoom / 2;
        _start = center - halfRange;
        _end = center + halfRange;
        clampRange();
        if (_start >= _end) _end = _start + 1; // Ensure _start is less than _end

    }

    int getStart() const { return _start; }
    int getEnd() const { return _end; }
    int getMin() const { return _min; }
    int getMax() const { return _max; }
    void setMax(int max) {_max = max; }

private:
    void clampRange() {
        if (_start < _min) {
            _start = _min;
            _end = _min + (_end - _start);
        }
        if (_end > _max) {
            _end = _max;
            _start = _max - (_end - _start);
        }
        if (_start < _min) _start = _min; // Re-check after adjusting _end
        if (_start >= _end) _end = _start + 1; // Ensure _start is less than _end
    }

    int _start;
    int _end;
    int _min;
    int _max;
};


#endif //WHISKERTOOLBOX_XAXIS_HPP
