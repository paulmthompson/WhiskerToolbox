#ifndef VIEWADAPTER_HPP
#define VIEWADAPTER_HPP

#include "CoreGeometry/boundingbox.hpp"

#include <cstddef>

struct ViewAdapter {
    virtual ~ViewAdapter() = default;
    [[nodiscard]] virtual BoundingBox getProjectionBounds() const = 0;
    virtual void getPerAxisZoom(float & zoom_x, float & zoom_y) const = 0;
    virtual void setPerAxisZoom(float zoom_x, float zoom_y) = 0;
    virtual void getPan(float & pan_x, float & pan_y) const = 0;
    virtual void setPan(float pan_x, float pan_y) = 0;
    [[nodiscard]] virtual float getPadding() const = 0;
    [[nodiscard]] virtual int viewportWidth() const = 0;
    [[nodiscard]] virtual int viewportHeight() const = 0;
    virtual void requestUpdate() = 0;
    virtual void applyBoxZoomToWorldRect(BoundingBox const & bounds) = 0;
};

#endif// VIEWADAPTER_HPP
