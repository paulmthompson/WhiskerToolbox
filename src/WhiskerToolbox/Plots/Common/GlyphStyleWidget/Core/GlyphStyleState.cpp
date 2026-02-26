#include "GlyphStyleState.hpp"

GlyphStyleState::GlyphStyleState(QObject * parent)
    : QObject(parent)
{
}

CorePlotting::GlyphType GlyphStyleState::glyphType() const
{
    return _data.glyph_type;
}

void GlyphStyleState::setGlyphType(CorePlotting::GlyphType type)
{
    if (_data.glyph_type != type) {
        _data.glyph_type = type;
        emit styleChanged();
    }
}

float GlyphStyleState::size() const
{
    return _data.size;
}

void GlyphStyleState::setSize(float size)
{
    if (_data.size != size) {
        _data.size = size;
        emit styleChanged();
    }
}

std::string const & GlyphStyleState::hexColor() const
{
    return _data.hex_color;
}

void GlyphStyleState::setHexColor(std::string const & color)
{
    if (_data.hex_color != color) {
        _data.hex_color = color;
        emit styleChanged();
    }
}

float GlyphStyleState::alpha() const
{
    return _data.alpha;
}

void GlyphStyleState::setAlpha(float alpha)
{
    if (_data.alpha != alpha) {
        _data.alpha = alpha;
        emit styleChanged();
    }
}

void GlyphStyleState::setStyle(CorePlotting::GlyphStyleData const & style)
{
    bool changed = (_data.glyph_type != style.glyph_type) ||
                   (_data.size != style.size) ||
                   (_data.hex_color != style.hex_color) ||
                   (_data.alpha != style.alpha);

    if (changed) {
        _data = style;
        emit styleChanged();
    }
}

void GlyphStyleState::setStyleSilent(CorePlotting::GlyphStyleData const & style)
{
    bool changed = (_data.glyph_type != style.glyph_type) ||
                   (_data.size != style.size) ||
                   (_data.hex_color != style.hex_color) ||
                   (_data.alpha != style.alpha);

    if (changed) {
        _data = style;
        emit styleUpdated();
    }
}
