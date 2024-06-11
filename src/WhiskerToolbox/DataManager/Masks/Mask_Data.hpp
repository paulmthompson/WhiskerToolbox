#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include <vector>
#include <map>


#include "Points/Point_Data.hpp"

using Mask2D = std::vector<Point2D<float>>;

class MaskData {
public:
    MaskData();
    void clearMasksAtTime(int const time);
    void addMaskAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y);
    void addMaskAtTime(int const time, std::vector<Point2D<float>> const mask);

    int getMaskHeight() const {return _mask_height;};
    int getMaskWidth() const {return _mask_width;};

    void setMaskHeight(int const height) {_mask_height = height;};
    void setMaskWidth(int const width) {_mask_width = width;};

    std::vector<Mask2D> const& getMasksAtTime(int const time) const;
protected:

private:
    std::map<int,std::vector<Mask2D>> _data;
    std::vector<Mask2D> _empty;
    int _mask_height;
    int _mask_width;

    Mask2D _createMask(std::vector<float> const& x, std::vector<float> const& y);
};


#endif // MASK_DATA_HPP
