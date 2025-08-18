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
};

#endif// IMAGESIZE_HPP
