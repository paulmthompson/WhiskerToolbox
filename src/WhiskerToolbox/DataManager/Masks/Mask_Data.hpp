#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "masks.hpp"

#include <ranges>
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
    void clearMasksAtTime(size_t time);
    void addMaskAtTime(size_t time, std::vector<float> const & x, std::vector<float> const & y);
    void addMaskAtTime(size_t time, std::vector<Point2D<float>> mask);

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

    [[nodiscard]] std::vector<Mask2D> const & getMasksAtTime(size_t time) const;

    /**
     * @brief Get all masks with their associated times as a range
     *
     * @return A view of time-mask pairs for all times
     */
    [[nodiscard]] auto getAllMasksAsRange() const {
        struct TimeMaskPair {
            int time;
            std::vector<Mask2D> const & masks;
        };

        return std::views::iota(size_t{0}, _time.size()) |
               std::views::transform([this](size_t i) {
                   return TimeMaskPair{static_cast<int>(_time[i]), _data[i]};
               });
    }

protected:
private:
    std::vector<size_t> _time;
    std::vector<std::vector<Mask2D>> _data;
    //std::map<int, std::vector<Mask2D>> _data;
    std::vector<Mask2D> _empty;
    ImageSize _image_size;
};


#endif// MASK_DATA_HPP
