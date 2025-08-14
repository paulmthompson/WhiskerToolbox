#ifndef EVENTPLOTVIEWADAPTER_HPP
#define EVENTPLOTVIEWADAPTER_HPP

#include "Analysis_Dashboard/Widgets/Common/ViewAdapter.hpp"

class EventPlotOpenGLWidget;

class EventPlotViewAdapter final : public ViewAdapter {
public:
    explicit EventPlotViewAdapter(EventPlotOpenGLWidget * widget);
    BoundingBox getProjectionBounds() const override;
    void getPerAxisZoom(float & zoom_x, float & zoom_y) const override;
    void setPerAxisZoom(float zoom_x, float zoom_y) override;
    void getPan(float & pan_x, float & pan_y) const override;
    void setPan(float pan_x, float pan_y) override;
    float getPadding() const override;
    int viewportWidth() const override;
    int viewportHeight() const override;
    void requestUpdate() override;
    void applyBoxZoomToWorldRect(BoundingBox const& bounds) override;

private:
    EventPlotOpenGLWidget * _w;
};

#endif // EVENTPLOTVIEWADAPTER_HPP

