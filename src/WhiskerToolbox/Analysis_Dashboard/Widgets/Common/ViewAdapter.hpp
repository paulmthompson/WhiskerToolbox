#ifndef VIEWADAPTER_HPP
#define VIEWADAPTER_HPP

#include <cstddef>

struct ViewAdapter {
    virtual ~ViewAdapter() = default;
    virtual void getProjectionBounds(float & left, float & right, float & bottom, float & top) const = 0;
    virtual void getPerAxisZoom(float & zoom_x, float & zoom_y) const = 0;
    virtual void setPerAxisZoom(float zoom_x, float zoom_y) = 0;
    virtual void getPan(float & pan_x, float & pan_y) const = 0;
    virtual void setPan(float pan_x, float pan_y) = 0;
    virtual float getPadding() const = 0;
    virtual int viewportWidth() const = 0;
    virtual int viewportHeight() const = 0;
    virtual void requestUpdate() = 0;
    virtual void applyBoxZoomToWorldRect(float min_x, float max_x, float min_y, float max_y) = 0;
};

#endif // VIEWADAPTER_HPP

