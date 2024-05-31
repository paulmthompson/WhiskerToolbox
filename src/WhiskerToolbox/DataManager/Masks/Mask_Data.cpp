#include "Mask_Data.hpp"

MaskData::MaskData()
{

}

void MaskData::clearMasksAtTime(const int time)
{
    _data[time].clear();
}

void MaskData::addMaskAtTime(const int time, const std::vector<float>& x, const std::vector<float>& y)
{
    auto new_mask = _createMask(x,y);

    _data[time].push_back(new_mask);
}

void MaskData::addMaskAtTime(const int time, const std::vector<Point2D> mask)
{
    _data[time].push_back(mask);
}

std::vector<Mask2D> MaskData::getMasksAtTime(const int time)
{
    return _data[time];
}

Mask2D MaskData::_createMask(const std::vector<float>& x, const std::vector<float>& y)
{
    auto new_mask = Mask2D{x.size()};

    for (int i = 0; i < x.size(); i++)
    {
        new_mask[i] = Point2D{x[i],y[i]};
    }

    return new_mask;
}
