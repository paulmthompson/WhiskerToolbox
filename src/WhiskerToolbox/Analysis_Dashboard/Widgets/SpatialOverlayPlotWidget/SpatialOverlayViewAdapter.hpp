#ifndef SPATIALOVERLAYVIEWADAPTER_HPP
#define SPATIALOVERLAYVIEWADAPTER_HPP

#include "Analysis_Dashboard/Widgets/Common/ViewAdapter.hpp"

class SpatialOverlayOpenGLWidget;

class SpatialOverlayViewAdapter final : public ViewAdapter {
public:
    explicit SpatialOverlayViewAdapter(SpatialOverlayOpenGLWidget * widget);
    void getProjectionBounds(float & left, float & right, float & bottom, float & top) const override;
    void getPerAxisZoom(float & zoom_x, float & zoom_y) const override;
    void setPerAxisZoom(float zoom_x, float zoom_y) override;
    void getPan(float & pan_x, float & pan_y) const override;
    void setPan(float pan_x, float pan_y) override;
    float getPadding() const override;
    int viewportWidth() const override;
    int viewportHeight() const override;
    void requestUpdate() override;
    void applyBoxZoomToWorldRect(float min_x, float max_x, float min_y, float max_y) override;

private:
    SpatialOverlayOpenGLWidget * _w;
};

#endif // SPATIALOVERLAYVIEWADAPTER_HPP

