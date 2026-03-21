#ifndef IMAGESIZE_HPP
#define IMAGESIZE_HPP

struct ImageSize {
    int width = -1;
    int height = -1;

    bool operator==(ImageSize const & other) const {
        return width == other.width && height == other.height;
    }

    bool operator!=(ImageSize const & other) const {
        return !(*this == other);
    }

    /// @brief Check if the image size is defined (not -1, -1)
    [[nodiscard]] bool isDefined() const {
        return width != -1 && height != -1;
    }
};

#endif// IMAGESIZE_HPP
