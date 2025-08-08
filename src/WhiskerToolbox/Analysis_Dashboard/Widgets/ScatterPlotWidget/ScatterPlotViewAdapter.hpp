#ifndef SCATTERPLOTVIEWADAPTER_HPP
#define SCATTERPLOTVIEWADAPTER_HPP

// Prototype header

// Project headers
#include "Analysis_Dashboard/Widgets/Common/ViewAdapter.hpp"

// Forward declarations (project)
class ScatterPlotOpenGLWidget;

// Standard headers

class ScatterPlotViewAdapter final : public ViewAdapter {
public:
    explicit ScatterPlotViewAdapter(ScatterPlotOpenGLWidget * widget);

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
    ScatterPlotOpenGLWidget * _w;
};

#endif // SCATTERPLOTVIEWADAPTER_HPP

