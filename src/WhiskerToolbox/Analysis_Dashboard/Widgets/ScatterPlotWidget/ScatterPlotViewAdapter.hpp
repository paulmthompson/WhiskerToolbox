#ifndef SCATTERPLOTVIEWADAPTER_HPP
#define SCATTERPLOTVIEWADAPTER_HPP

#include "../Common/GenericViewAdapter.hpp"

class ScatterPlotOpenGLWidget;

/**
 * @brief View adapter for ScatterPlotOpenGLWidget
 * 
 * This is now just a thin wrapper around GenericViewAdapter since all the
 * common functionality has been moved to ViewState and ViewUtils.
 */
class ScatterPlotViewAdapter final : public GenericViewAdapter {
public:
    explicit ScatterPlotViewAdapter(ScatterPlotOpenGLWidget* widget);
};

#endif // SCATTERPLOTVIEWADAPTER_HPP

