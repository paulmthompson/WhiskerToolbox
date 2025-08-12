#ifndef SPATIALOVERLAYVIEWADAPTER_HPP
#define SPATIALOVERLAYVIEWADAPTER_HPP

#include "../Common/GenericViewAdapter.hpp"

class SpatialOverlayOpenGLWidget;

/**
 * @brief View adapter for SpatialOverlayOpenGLWidget
 * 
 * This is now just a thin wrapper around GenericViewAdapter since all the
 * common functionality has been moved to ViewState and ViewUtils.
 */
class SpatialOverlayViewAdapter final : public GenericViewAdapter {
public:
    explicit SpatialOverlayViewAdapter(SpatialOverlayOpenGLWidget* widget);
};

#endif // SPATIALOVERLAYVIEWADAPTER_HPP

