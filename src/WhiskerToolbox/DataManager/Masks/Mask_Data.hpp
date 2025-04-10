#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "masks.hpp"

#include <map>
#include <vector>

/**
 * @brief The MaskData class
 *
 * MaskData is used for 2D data where the collection of 2D points has *no* order
 * Compare to LineData where the collection of 2D points has an order
 *
 */
class MaskData : public ObserverData {
public:
    MaskData() = default;
    void clearMasksAtTime(int time);
    void addMaskAtTime(int time, std::vector<float> const & x, std::vector<float> const & y);
    void addMaskAtTime(int time, std::vector<Point2D<float>> mask);

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

    [[nodiscard]] std::vector<Mask2D> const & getMasksAtTime(int time) const;
    std::map<int, std::vector<Mask2D>> getData() { return _data; };

protected:
private:
    std::map<int, std::vector<Mask2D>> _data;
    std::vector<Mask2D> _empty;
    ImageSize _image_size;
};


#endif// MASK_DATA_HPP
