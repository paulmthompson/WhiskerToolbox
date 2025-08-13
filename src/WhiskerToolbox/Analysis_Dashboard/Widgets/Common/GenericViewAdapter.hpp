#pragma once

#include "CoreGeometry/boundingbox.hpp"
#include "ViewAdapter.hpp"
#include "ViewState.hpp"

class BasePlotOpenGLWidget;

/**
 * @brief Generic view adapter that works with any BasePlotOpenGLWidget
 * 
 * This adapter uses the ViewState struct and ViewUtils functions to provide
 * common view functionality without code duplication. It can be used for any
 * widget that inherits from BasePlotOpenGLWidget.
 */
class GenericViewAdapter : public ViewAdapter {
public:
    explicit GenericViewAdapter(BasePlotOpenGLWidget * widget);

    BoundingBox getProjectionBounds() const override;
    void getPerAxisZoom(float & zoom_x, float & zoom_y) const override;
    void setPerAxisZoom(float zoom_x, float zoom_y) override;
    void getPan(float & pan_x, float & pan_y) const override;
    void setPan(float pan_x, float pan_y) override;
    float getPadding() const override;
    int viewportWidth() const override;
    int viewportHeight() const override;
    void requestUpdate() override;
    void applyBoxZoomToWorldRect(BoundingBox const & bounds) override;

private:
    BasePlotOpenGLWidget * _widget;
};
