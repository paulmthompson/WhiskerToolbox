#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include <vector>
#include <map>


#include "Points/Point_Data.hpp"

using Mask2D = std::vector<Point2D>;

class MaskData {
public:
    MaskData();
    void clearMasksAtTime(const int time);
    void addMaskAtTime(const int time, const std::vector<float>& x, const std::vector<float>& y);
    void addMaskAtTime(const int time, const std::vector<Point2D> mask);

    std::vector<Mask2D> const& getMasksAtTime(const int time) const;
protected:

private:
    std::map<int,std::vector<Mask2D>> _data;

    Mask2D _createMask(const std::vector<float>& x, const std::vector<float>& y);
};


#endif // MASK_DATA_HPP
