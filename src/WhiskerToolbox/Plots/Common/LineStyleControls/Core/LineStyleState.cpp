#include "LineStyleState.hpp"

LineStyleState::LineStyleState(QObject * parent)
    : QObject(parent)
{
}

std::string const & LineStyleState::hexColor() const
{
    return _data.hex_color;
}

void LineStyleState::setHexColor(std::string const & color)
{
    if (_data.hex_color != color) {
        _data.hex_color = color;
        emit styleChanged();
    }
}

float LineStyleState::thickness() const
{
    return _data.thickness;
}

void LineStyleState::setThickness(float thickness)
{
    if (_data.thickness != thickness) {
        _data.thickness = thickness;
        emit styleChanged();
    }
}

float LineStyleState::alpha() const
{
    return _data.alpha;
}

void LineStyleState::setAlpha(float alpha)
{
    if (_data.alpha != alpha) {
        _data.alpha = alpha;
        emit styleChanged();
    }
}

void LineStyleState::setStyle(CorePlotting::LineStyleData const & style)
{
    bool const changed = (_data.hex_color != style.hex_color) ||
                         (_data.thickness != style.thickness) ||
                         (_data.alpha != style.alpha);

    if (changed) {
        _data = style;
        emit styleChanged();
    }
}

void LineStyleState::setStyleSilent(CorePlotting::LineStyleData const & style)
{
    bool const changed = (_data.hex_color != style.hex_color) ||
                         (_data.thickness != style.thickness) ||
                         (_data.alpha != style.alpha);

    if (changed) {
        _data = style;
        emit styleUpdated();
    }
}
