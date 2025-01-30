#ifndef IMAGESIZE_HPP
#define IMAGESIZE_HPP



class ImageSize {
public:
    ImageSize() : _width(-1), _height(-1) {}
    ImageSize(int width, int height) : _width(width), _height(height) {}

    int getWidth() const { return _width; }
    int getHeight() const { return _height; }

    void setWidth(int width) { _width = width; }
    void setHeight(int height) { _height = height; }

private:
    int _width;
    int _height;
};

#endif // IMAGESIZE_HPP
